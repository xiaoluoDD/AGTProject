#include "tcpclient.h"
#include "ui_tcpclient.h"
#include <QMessageBox>
#include <QDateTime>
#include <QTextCursor>
#include <QHeaderView>

/**
 * @brief 构造函数
 * @param parent 父窗口指针
 */
tcpClient::tcpClient(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::tcpClient)
    , m_socket(new QTcpSocket(this))
    , m_connectionTimer(new QTimer(this))
    , m_isConnected(false)
    , m_fullTrayCount(100)
{
    ui->setupUi(this);
    setupUI();
    setupTable();
    
    // 连接Socket信号槽
    connect(m_socket, &QTcpSocket::connected, this, &tcpClient::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &tcpClient::onSocketDisconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &tcpClient::onSocketError);
    connect(m_socket, &QTcpSocket::readyRead, this, &tcpClient::onSocketReadyRead);
    
    // 连接超时定时器
    connect(m_connectionTimer, &QTimer::timeout, this, &tcpClient::onConnectionTimeout);
    
    // 设置连接超时时间（5秒）
    m_connectionTimer->setSingleShot(true);
    m_connectionTimer->setInterval(5000);
    
    updateConnectionStatus(false);
}

/**
 * @brief 析构函数
 */
tcpClient::~tcpClient()
{
    // 断开连接并清理资源
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    }
    delete ui;
}

/**
 * @brief 初始化用户界面
 */
void tcpClient::setupUI()
{
    setWindowTitle("AGT滑槽记录表");
    
    // 设置默认连接参数
    ui->lineEditServerIP->setText("127.0.0.1");
    ui->lineEditPort->setText("8080");
    
    // 设置满托盘数量默认值
    ui->lineEditFullTrayCount->setText(QString::number(m_fullTrayCount));
    
    // 设置日志区域为只读
    ui->textEditLog->setReadOnly(true);
    ui->textEditSend->setPlaceholderText("请输入要发送的数据...");
    
    // 连接界面切换按钮信号槽
    connect(ui->pushButtonConnectionPage, &QPushButton::clicked, this, &tcpClient::onConnectionPageClicked);
    connect(ui->pushButtonTablePage, &QPushButton::clicked, this, &tcpClient::onTablePageClicked);
    
    // 连接按钮信号槽
    connect(ui->pushButtonConnect, &QPushButton::clicked, this, &tcpClient::onConnectClicked);
    connect(ui->pushButtonDisconnect, &QPushButton::clicked, this, &tcpClient::onDisconnectClicked);
    connect(ui->pushButtonSend, &QPushButton::clicked, this, &tcpClient::onSendClicked);
    connect(ui->pushButtonClear, &QPushButton::clicked, this, &tcpClient::onClearClicked);
    
    // 连接表格操作按钮信号槽
    connect(ui->pushButtonClearTable, &QPushButton::clicked, this, &tcpClient::onClearTableClicked);
    connect(ui->pushButtonExportTable, &QPushButton::clicked, this, &tcpClient::onExportTableClicked);
    
    // 连接设置相关按钮信号槽
    connect(ui->pushButtonSetFullTrayCount, &QPushButton::clicked, this, &tcpClient::onSetFullTrayCountClicked);
    
    // 初始化按钮状态
    ui->pushButtonDisconnect->setEnabled(false);
    ui->pushButtonSend->setEnabled(false);
    ui->textEditSend->setEnabled(false);
    
    // 设置初始界面为连接界面
    ui->stackedWidget->setCurrentIndex(0);
}

/**
 * @brief 初始化表格
 */
void tcpClient::setupTable()
{
    // 设置表格列数和标题
    ui->tableWidget->setColumnCount(6);
    QStringList headers;
    headers << "车辆型号" << "数量" << "满盘进入时间" << "满盘送出时间" << "空盘进入时间" << "空盘送出时间";
    ui->tableWidget->setHorizontalHeaderLabels(headers);
    
    // 设置表格属性
    ui->tableWidget->setAlternatingRowColors(true);
    ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidget->setSortingEnabled(true);
    
    // 设置列宽 - 每列间距保持一致
    for (int i = 0; i < 6; ++i) {
        ui->tableWidget->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
    }
}

/**
 * @brief 切换到连接界面
 */
void tcpClient::onConnectionPageClicked()
{
    ui->stackedWidget->setCurrentIndex(0);
    ui->pushButtonConnectionPage->setChecked(true);
    ui->pushButtonTablePage->setChecked(false);
}

/**
 * @brief 切换到表格界面
 */
void tcpClient::onTablePageClicked()
{
    ui->stackedWidget->setCurrentIndex(1);
    ui->pushButtonConnectionPage->setChecked(false);
    ui->pushButtonTablePage->setChecked(true);
}

/**
 * @brief 设置满托盘数量按钮点击处理
 */
void tcpClient::onSetFullTrayCountClicked()
{
    QString countStr = ui->lineEditFullTrayCount->text().trimmed();
    
    // 验证输入
    if (countStr.isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入满托盘时数量");
        return;
    }
    
    bool ok;
    int count = countStr.toInt(&ok);
    if (!ok || count <= 0) {
        QMessageBox::warning(this, "警告", "请输入有效的正整数");
        return;
    }
    
    // 设置满托盘数量
    m_fullTrayCount = count;
    
    // 显示确认信息
    QMessageBox::information(this, "设置成功", 
                           QString("满托盘时数量已设置为: %1").arg(count));
    
    // 记录到日志
    appendToLog(QString("满托盘时数量设置为: %1").arg(count), false);
}

/**
 * @brief 清空表格按钮点击处理
 */
void tcpClient::onClearTableClicked()
{
    int rowCount = ui->tableWidget->rowCount();
    if (rowCount > 0) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "确认清空", 
            QString("确定要清空表格中的所有 %1 条记录吗？").arg(rowCount),
            QMessageBox::Yes | QMessageBox::No
        );
        
        if (reply == QMessageBox::Yes) {
            ui->tableWidget->setRowCount(0);
        }
    }
}

/**
 * @brief 导出表格按钮点击处理
 */
void tcpClient::onExportTableClicked()
{
    // 暂时显示提示信息，后续可以添加导出功能
    QMessageBox::information(this, "导出功能", "导出功能将在后续版本中实现");
}

/**
 * @brief 连接按钮点击处理
 */
void tcpClient::onConnectClicked()
{
    QString serverIP = ui->lineEditServerIP->text().trimmed();
    QString portStr = ui->lineEditPort->text().trimmed();
    
    // 验证IP地址
    if (serverIP.isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入服务器IP地址");
        return;
    }
    
    // 验证端口号
    bool ok;
    int port = portStr.toInt(&ok);
    if (!ok || port <= 0 || port > 65535) {
        QMessageBox::warning(this, "警告", "请输入有效的端口号(1-65535)");
        return;
    }
    
    // 开始连接
    appendToLog(QString("正在连接到 %1:%2...").arg(serverIP).arg(port));
    m_socket->connectToHost(serverIP, port);
    m_connectionTimer->start();
    
    ui->pushButtonConnect->setEnabled(false);
}

/**
 * @brief 断开按钮点击处理
 */
void tcpClient::onDisconnectClicked()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
        appendToLog("正在断开连接...");
    }
}

/**
 * @brief 发送按钮点击处理
 */
void tcpClient::onSendClicked()
{
    QString data = ui->textEditSend->toPlainText().trimmed();
    if (data.isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入要发送的数据");
        return;
    }
    
    if (!m_isConnected) {
        QMessageBox::warning(this, "警告", "请先连接到服务器");
        return;
    }
    
    sendData(data.toUtf8());
}

/**
 * @brief 清空日志按钮点击处理
 */
void tcpClient::onClearClicked()
{
    ui->textEditLog->clear();
}

/**
 * @brief Socket连接成功处理
 */
void tcpClient::onSocketConnected()
{
    m_connectionTimer->stop();
    m_isConnected = true;
    updateConnectionStatus(true);
    appendToLog("连接成功！", false);
}

/**
 * @brief Socket断开连接处理
 */
void tcpClient::onSocketDisconnected()
{
    m_connectionTimer->stop();
    m_isConnected = false;
    updateConnectionStatus(false);
    appendToLog("连接已断开", false);
}

/**
 * @brief Socket错误处理
 * @param error 错误类型
 */
void tcpClient::onSocketError(QAbstractSocket::SocketError error)
{
    m_connectionTimer->stop();
    m_isConnected = false;
    updateConnectionStatus(false);
    
    QString errorMsg = m_socket->errorString();
    appendToLog(QString("连接错误: %1").arg(errorMsg), true);
}

/**
 * @brief Socket数据可读处理
 */
void tcpClient::onSocketReadyRead()
{
    QByteArray data = m_socket->readAll();
    processHexData(data);
}

/**
 * @brief 连接超时处理
 */
void tcpClient::onConnectionTimeout()
{
    m_socket->abort();
    appendToLog("连接超时", true);
    updateConnectionStatus(false);
}

/**
 * @brief 更新连接状态显示
 * @param connected 连接状态
 */
void tcpClient::updateConnectionStatus(bool connected)
{
    if (connected) {
        // 已连接状态
        ui->labelStatus->setText("已连接");
        ui->labelStatus->setStyleSheet("color: green; font-weight: bold;");
        ui->pushButtonConnect->setEnabled(false);
        ui->pushButtonDisconnect->setEnabled(true);
        ui->pushButtonSend->setEnabled(true);
        ui->textEditSend->setEnabled(true);
        ui->lineEditServerIP->setEnabled(false);
        ui->lineEditPort->setEnabled(false);
    } else {
        // 未连接状态
        ui->labelStatus->setText("未连接");
        ui->labelStatus->setStyleSheet("color: red; font-weight: bold;");
        ui->pushButtonConnect->setEnabled(true);
        ui->pushButtonDisconnect->setEnabled(false);
        ui->pushButtonSend->setEnabled(false);
        ui->textEditSend->setEnabled(false);
        ui->lineEditServerIP->setEnabled(true);
        ui->lineEditPort->setEnabled(true);
    }
}

/**
 * @brief 添加日志信息
 * @param message 日志消息
 * @param isError 是否为错误信息
 */
void tcpClient::appendToLog(const QString &message, bool isError)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString logMessage = QString("[%1] %2").arg(timestamp).arg(message);
    
    // 设置光标位置到末尾
    QTextCursor cursor = ui->textEditLog->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->textEditLog->setTextCursor(cursor);
    
    // 设置文本颜色
    if (isError) {
        ui->textEditLog->setTextColor(Qt::red);
    } else {
        ui->textEditLog->setTextColor(Qt::black);
    }
    
    ui->textEditLog->insertPlainText(logMessage + "\n");
    
    // 自动滚动到底部
    QTextCursor scrollCursor = ui->textEditLog->textCursor();
    scrollCursor.movePosition(QTextCursor::End);
    ui->textEditLog->setTextCursor(scrollCursor);
}

/**
 * @brief 发送数据到服务器
 * @param data 要发送的数据
 */
void tcpClient::sendData(const QByteArray &data)
{
    if (m_socket->write(data) == -1) {
        appendToLog("发送数据失败", true);
        return;
    }
    
    // 显示发送的数据
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString logMessage = QString("[%1] 发送: %2").arg(timestamp).arg(QString::fromUtf8(data));
    appendToLog(logMessage, false);
    
    ui->textEditSend->clear();
}

/**
 * @brief 处理16进制数据
 * @param data 接收到的原始数据
 */
void tcpClient::processHexData(const QByteArray &data)
{
    // 检查数据长度是否至少4字节
    if (data.size() < 4) {
        appendToLog(QString("接收数据长度不足: %1 字节").arg(data.size()), true);
        return;
    }
    
    // 检查前4个字节是否为固定的 06 00 42 00
    QByteArray header = data.left(4);
    QByteArray expectedHeader = QByteArray::fromHex("06004200");
    
    if (header != expectedHeader) {
        appendToLog(QString("接收数据头部不匹配，期望: 06 00 42 00，实际: %1")
                   .arg(header.toHex(' ').toUpper()), true);
        return;
    }
    
    // 转换为16进制字符串显示
    QString hexString = data.toHex(' ').toUpper();
    QString hexDisplay = QString("接收数据(HEX): %1").arg(hexString);
    
    // 转换为10进制字符串显示
    QString decimalDisplay = "接收数据(DEC): ";
    for (int i = 0; i < data.size(); ++i) {
        if (i > 0) decimalDisplay += " ";
        decimalDisplay += QString::number(static_cast<unsigned char>(data.at(i)));
    }
    
    // 显示接收到的数据
    appendToLog(QString("接收: 数据长度 %1 字节").arg(data.size()), false);
    appendToLog(hexDisplay, false);
    appendToLog(decimalDisplay, false);
    
    // 如果有额外的数据（超过4字节），显示载荷信息
    if (data.size() > 4) {
        QByteArray payload = data.mid(4);
        QString payloadHex = payload.toHex(' ').toUpper();
        QString payloadDecimal;
        
        for (int i = 0; i < payload.size(); ++i) {
            if (i > 0) payloadDecimal += " ";
            payloadDecimal += QString::number(static_cast<unsigned char>(payload.at(i)));
        }
        
        appendToLog(QString("数据载荷(HEX): %1").arg(payloadHex), false);
        appendToLog(QString("数据载荷(DEC): %1").arg(payloadDecimal), false);
    }
}
