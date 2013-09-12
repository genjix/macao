#ifndef BEGINWINDOW_H
#define BEGINWINDOW_H

#include <thread>
#include <mutex>
#include <Qt/QtGui>
#include <bitcoin/bitcoin.hpp>
#include <obelisk/obelisk.hpp>

extern std::mutex broadcast_mutex;
extern std::vector<bc::transaction_type> tx_broadcast_queue;

class BeginWindow
  : public QWidget
{
    Q_OBJECT

private slots:
    void begin();

public:
    BeginWindow(obelisk::fullnode_interface& fullnode);
    void reset();

private:
    obelisk::fullnode_interface& fullnode_;
};

class SendDialog
  : public QWidget
{
    Q_OBJECT

private slots:
    void paybar();
    void payqr();
    void payaddr();

public:
    SendDialog();

private:
    void continue_payment();

    QLineEdit* enter_amount_;
    QPushButton* paybar_button_;
    QPushButton* payqr_button_;
    QPushButton* payaddr_button_;
};

#endif

