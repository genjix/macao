#include "beginwindow.h"

#include <iostream>
#include <future>
#include "pstream.h"

const std::string bar_addr = "1Fufjpf9RM2aQsGedhSpbSCGRHrmLMJ7yY";

// warning: this is not good code!
std::mutex broadcast_mutex;
std::vector<bc::transaction_type> tx_broadcast_queue;

BeginWindow* mainwin = nullptr;

constexpr uint64_t fee = 10000;

struct current_payment_details
{
    SendDialog* dialog = nullptr;
    bc::blockchain::history_list history;
    bc::payment_address origin_addr;
    bc::payment_address dest_addr;
    uint64_t balance;
    uint64_t amount;
    bc::secret_parameter secret;
} currpay;

BeginWindow::BeginWindow(obelisk::fullnode_interface& fullnode)
  : fullnode_(fullnode)
{
    mainwin = this; // Bite me.
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    QPushButton* start_button = new QPushButton(tr("Make Payment"));
    connect(start_button, SIGNAL(clicked()), this, SLOT(begin()));
    main_layout->addWidget(start_button);
    resize(500, 400);
    show();
}

void BeginWindow::reset()
{
    delete currpay.dialog;
    currpay.history.clear();
    show();
}

void BeginWindow::begin()
{
    hide();
    redi::ipstream proc("python scan-seed.py");
    std::string line, l;
    while (std::getline(proc.out(), l))
        line += l;
    if (line == "FAIL")
    {
        QMessageBox errmsg;
        errmsg.critical(0, tr("Error"), tr("Failed to scan QR code!"));
        show();
        return;
    }
    else if (line == "NOTSEED")
    {
        QMessageBox errmsg;
        errmsg.critical(0, tr("Error"), tr("Show your private seed, not the receiving address."));
        show();
        return;
    }
    else if (line == "BADSEED")
    {
        QMessageBox errmsg;
        errmsg.critical(0, tr("Error"), tr("Seed is incorrectly formed."));
        show();
        return;
    }
    bc::deterministic_wallet detwallet;
    bool set_seed_success = detwallet.set_seed(line);
    assert(set_seed_success);
    // 1FvS9vyhpdtCAA5RQppUaFCxE9DJFr6jf9
    bc::payment_address addr;
    set_public_key(addr, detwallet.generate_public_key(0));
    currpay.secret = detwallet.generate_secret(0);
    // Use a promise?
    std::promise<std::error_code> ec_chain;
    auto history_fetched =
        [&](const std::error_code& ec,
            const bc::blockchain::history_list& history)
        {
            currpay.history = history;
            ec_chain.set_value(ec);
        };
    currpay.origin_addr = addr;
    fullnode_.address.fetch_history(addr, history_fetched);
    std::error_code ec = ec_chain.get_future().get();
    if (ec)
    {
        QMessageBox errmsg;
        errmsg.critical(0, tr("Error"), tr("Error fetching history! Maybe blockchain server is down. :("));
        std::cerr << "Server said: " << ec.message() << std::endl;
        show();
        return;
    }
    currpay.dialog = new SendDialog();
    currpay.dialog->show();
}

SendDialog::SendDialog()
{
    currpay.balance = 0;
    for (const auto& row: currpay.history)
    {
        if (row.spend.hash != bc::null_hash)
            continue;
        currpay.balance += row.value;
    }
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->addWidget(new QLabel(
        (std::string("Balance: ") + bc::satoshi_to_btc(currpay.balance) + " BTC").c_str()));
    QHBoxLayout* amount_part = new QHBoxLayout();
    amount_part->addWidget(new QLabel("Amount:"));
    enter_amount_ = new QLineEdit();
    amount_part->addWidget(enter_amount_);
    main_layout->addLayout(amount_part);
    paybar_button_ = new QPushButton(tr("Pay Bar"));
    payqr_button_ = new QPushButton(tr("Pay to QR Code"));
    payaddr_button_ = new QPushButton(tr("Enter Address"));
    main_layout->addWidget(paybar_button_);
    main_layout->addWidget(payqr_button_);
    main_layout->addWidget(payaddr_button_);
    connect(paybar_button_, SIGNAL(clicked()), this, SLOT(paybar()));
    connect(payqr_button_, SIGNAL(clicked()), this, SLOT(payqr()));
    connect(payaddr_button_, SIGNAL(clicked()), this, SLOT(payaddr()));
}

void SendDialog::paybar()
{
    currpay.dest_addr.set_encoded(bar_addr);
    continue_payment();
}
void SendDialog::payqr()
{
    redi::ipstream proc("python scan-addr.py");
    std::string line, l;
    while (std::getline(proc.out(), l))
        line += l;
    if (line == "FAIL")
    {
        QMessageBox errmsg;
        errmsg.critical(0, tr("Error"), tr("Failed to scan QR code!"));
        show();
        mainwin->reset();
        return;
    }
    else if (line == "NOTADDR")
    {
        QMessageBox errmsg;
        errmsg.critical(0, tr("Error"), tr("Show your receiving address, not the seed."));
        show();
        mainwin->reset();
        return;
    }
    if (!currpay.dest_addr.set_encoded(line))
    {
        QMessageBox errmsg;
        errmsg.critical(0, tr("Error"), tr("Invalid address shown."));
        show();
        mainwin->reset();
        return;
    }
    continue_payment();
}
void SendDialog::payaddr()
{
    bool ok;
    QString text = QInputDialog::getText(
        this, tr("Enter Destination Address"),
        tr("Address: "), QLineEdit::Normal,
        "", &ok);
    if (!ok || text.isEmpty())
    {
        QMessageBox errmsg;
        errmsg.critical(0, tr("Error"), tr("Problem entering the address?"));
        show();
        mainwin->reset();
        return;
    }
    std::string addr = text.toUtf8().constData();
    if (!currpay.dest_addr.set_encoded(addr))
    {
        QMessageBox errmsg;
        errmsg.critical(0, tr("Error"), tr("Invalid address enetered!"));
        show();
        mainwin->reset();
        return;
    }
    continue_payment();
}

using namespace bc;

// Maybe should also be in libbitcoin too?
script_type build_pubkey_hash_script(const short_hash& pubkey_hash)
{
    script_type result;
    result.push_operation({opcode::dup, data_chunk()});
    result.push_operation({opcode::hash160, data_chunk()});
    result.push_operation({opcode::special,
        data_chunk(pubkey_hash.begin(), pubkey_hash.end())});
    result.push_operation({opcode::equalverify, data_chunk()});
    result.push_operation({opcode::checksig, data_chunk()});
    return result;
}

script_type build_script_hash_script(const short_hash& script_hash)
{
    script_type result;
    result.push_operation({opcode::hash160, data_chunk()});
    result.push_operation({opcode::special,
        data_chunk(script_hash.begin(), script_hash.end())});
    result.push_operation({opcode::equal, data_chunk()});
    return result;
}

bool build_output_script(
    script_type& out_script, const payment_address& payaddr)
{
    switch (payaddr.version())
    {
        case payment_address::pubkey_version:
            out_script = build_pubkey_hash_script(payaddr.hash());
            return true;

        case payment_address::script_version:
            out_script = build_script_hash_script(payaddr.hash());
            return true;
    }
    return false;
}

bool make_signature(transaction_type& tx, size_t input_index,
    const elliptic_curve_key& key, const script_type& script_code)
{
    transaction_input_type& input = tx.inputs[input_index];

    const data_chunk public_key = key.public_key();
    if (public_key.empty())
        return false;
    hash_digest tx_hash =
        script_type::generate_signature_hash(tx, input_index, script_code, 1);
    if (tx_hash == null_hash)
        return false;
    data_chunk signature = key.sign(tx_hash);
    signature.push_back(0x01);
    //std::cout << signature << std::endl;
    script_type& script = tx.inputs[input_index].script;
    // signature
    script.push_operation({opcode::special, signature});
    // public key
    script.push_operation({opcode::special, public_key});
    return true;
}

void SendDialog::continue_payment()
{
    std::string convcmd = std::string("sx satoshi ") + enter_amount_->text().toUtf8().constData();
    redi::ipstream proc(convcmd);
    std::string line, l;
    while (std::getline(proc.out(), l))
        line += l;
    try
    {
        currpay.amount = boost::lexical_cast<uint64_t>(line);
    }
    catch (const boost::bad_lexical_cast&)
    {
        QMessageBox errmsg;
        errmsg.critical(0, tr("Error"), tr("Invalid amount to send entered."));
        show();
        mainwin->reset();
        return;
    }
    if (currpay.amount < fee)
    {
        QMessageBox errmsg;
        errmsg.critical(0, tr("Error"), tr("Your funds are too small and will be stuck in limbo."));
        show();
        mainwin->reset();
        return;
    }
    // Allow people to send their total balance.
    if (currpay.amount == currpay.balance)
    {
        currpay.amount = currpay.balance - fee;
    }
    std::cout << "Sending " << currpay.amount << " to " << currpay.dest_addr.encoded() << std::endl;
    uint64_t value = currpay.amount + fee;
    if (currpay.amount > value)
    {
        QMessageBox errmsg;
        errmsg.critical(0, tr("Error"), tr("Not enough money for payment."));
        show();
        mainwin->reset();
        return;
    }
    // ready to send!
    output_info_list outs;
    for (const auto& row: currpay.history)
    {
        if (row.spend.hash != bc::null_hash)
            continue;
        outs.push_back({row.output, row.value});
    }
    select_outputs_result unspent = bc::select_outputs(outs, value);
    if (unspent.points.empty())
    {
        QMessageBox errmsg;
        errmsg.critical(0, tr("Error"), tr("Internal error. Notify amir."));
        show();
        mainwin->reset();
        return;
    }
    // construct transaction now.
    transaction_type tx;
    tx.version = 1;
    tx.locktime = 0;
    // start with outputs.
    // dest addr output first.
    transaction_output_type dest_output;
    dest_output.value = currpay.amount;
    if (!build_output_script(dest_output.script, currpay.dest_addr))
    {
        QMessageBox errmsg;
        errmsg.critical(0, tr("Error"), tr("send: Unsupported address type."));
        show();
        mainwin->reset();
        return;
    }
    tx.outputs.push_back(dest_output);
    // add change output also.
    transaction_output_type change_output;
    change_output.value = unspent.change;
    //bc::payment_address change_addr = control.change_address();
    bool change_script_success =
        build_output_script(change_output.script, currpay.origin_addr);
    BITCOIN_ASSERT(change_script_success);
    tx.outputs.push_back(change_output);
    // notice we have left the fee out.
    // now do inputs.
    for (const bc::output_point& prevout: unspent.points)
    {
        transaction_input_type input;
        input.previous_output = prevout;
        input.sequence = 4294967295;
        tx.inputs.push_back(input);
    }
    // now sign inputs
    for (size_t i = 0; i < tx.inputs.size(); ++i)
    {
        bc::transaction_input_type& input = tx.inputs[i];
        elliptic_curve_key key;
        bool set_secret_success = key.set_secret(currpay.secret);
        BITCOIN_ASSERT(set_secret_success);
        payment_address address;
        set_public_key(address, key.public_key());
        BITCOIN_ASSERT(address == currpay.origin_addr);
        script_type prevout_script_code;
        bool prevout_script_code_success =
            build_output_script(prevout_script_code, address);
        BITCOIN_ASSERT(prevout_script_code_success);
        bool sign_success = make_signature(tx, i, key, prevout_script_code);
    }
    // holy shit! now lets broadcast the tx!
    broadcast_mutex.lock();
    tx_broadcast_queue.push_back(tx);
    broadcast_mutex.unlock();
    sleep(1);
    std::string resinfo = std::string("Sent ") +
        bc::satoshi_to_btc(currpay.amount) + " BTC to " +
        currpay.dest_addr.encoded();
    resinfo += "\n";
    std::string tx_hash = bc::encode_hex(bc::hash_transaction(tx));
    resinfo += "<a href='http://blockchain.info/tx/" + tx_hash +
        "'>" + tx_hash + "</a>";
    QMessageBox msg;
    msg.setWindowTitle("Money Sent!");
    msg.setTextFormat(Qt::RichText);
    msg.setText(resinfo.c_str());
    msg.exec();
    mainwin->reset();
}

