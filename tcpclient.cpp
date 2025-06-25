#include "tcpclient.h"
#include "ui_tcpclient.h"

tcpClient::tcpClient(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::tcpClient)
{
    ui->setupUi(this);
}

tcpClient::~tcpClient()
{
    delete ui;
}
