#include "tcpclient.h"
#include "./ui_tcpclient.h"
#include <QMessageBox>
#include <QDateTime>
#include <QTextCursor>
#include <QHeaderView>
#include <QFileDialog>
#include <QTextStream>
#include <QTableWidgetItem>
#include <QSet>
#include <algorithm>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QCoreApplication>

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
    , m_fullTrayCount(0)
{
    ui->setupUi(this);
    setupUI();
    setupTable();
    setupVehicleBindingTable();
    initDatabase();
    loadModelBindingsFromDb();
    loadDataRecordsFromDb();
    
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
    
    appendToLog("TCP客户端已启动");
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
    
    // 设置日志区域为只读
    ui->textEditLog->setReadOnly(true);
    ui->textEditSend->setPlaceholderText("请输入要发送的数据...");
    
    // 连接界面切换按钮信号槽
    connect(ui->pushButtonConnectionPage, &QPushButton::clicked, this, &tcpClient::onConnectionPageClicked);
    connect(ui->pushButtonTablePage, &QPushButton::clicked, this, &tcpClient::onTablePageClicked);
    connect(ui->pushButtonVehicleBindingPage, &QPushButton::clicked, this, &tcpClient::onVehicleBindingPageClicked);
    
    // 连接按钮信号槽
    connect(ui->pushButtonConnect, &QPushButton::clicked, this, &tcpClient::onConnectClicked);
    connect(ui->pushButtonDisconnect, &QPushButton::clicked, this, &tcpClient::onDisconnectClicked);
    connect(ui->pushButtonSend, &QPushButton::clicked, this, &tcpClient::onSendClicked);
    connect(ui->pushButtonClear, &QPushButton::clicked, this, &tcpClient::onClearClicked);
    
    // 连接表格操作按钮信号槽
    connect(ui->pushButtonClearTable, &QPushButton::clicked, this, &tcpClient::onClearTableClicked);
    connect(ui->pushButtonExportTable, &QPushButton::clicked, this, &tcpClient::onExportTableClicked);
    
    // 连接车型绑定表格操作按钮信号槽
    connect(ui->pushButtonAddVehicle, &QPushButton::clicked, this, &tcpClient::onAddVehicleClicked);
    connect(ui->pushButtonDeleteVehicle, &QPushButton::clicked, this, &tcpClient::onDeleteVehicleClicked);
    connect(ui->pushButtonClearVehicleTable, &QPushButton::clicked, this, &tcpClient::onClearVehicleTableClicked);
    connect(ui->pushButtonExportVehicleTable, &QPushButton::clicked, this, &tcpClient::onExportVehicleTableClicked);
    
    // 连接数据解析信号槽
    connect(this, &tcpClient::dataParsed, this, &tcpClient::onDataParsed);
    
    // 初始化按钮状态
    ui->pushButtonDisconnect->setEnabled(false);
    ui->pushButtonSend->setEnabled(false);
    ui->textEditSend->setEnabled(false);
    
    // 设置初始界面为连接界面
    ui->stackedWidget->setCurrentIndex(0);
    
    ui->pushButtonExportTable->setVisible(false);
}

/**
 * @brief 初始化表格
 */
void tcpClient::setupTable()
{
    // 设置表格列数和标题
    ui->tableWidget->setColumnCount(6);
    QStringList headers;
    headers << "滑槽号" << "送入送出状态" << "车型代码" << "车型名称" << "数量（件）" << "时间";
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
 * @brief 初始化车型绑定表格
 */
void tcpClient::setupVehicleBindingTable()
{
    // 设置表格列数和标题
    ui->tableWidgetVehicleBinding->setColumnCount(4);
    QStringList headers;
    headers << "序号" << "车型代码" << "车型名称" << "数量";
    ui->tableWidgetVehicleBinding->setHorizontalHeaderLabels(headers);
    
    // 设置表格属性
    ui->tableWidgetVehicleBinding->setAlternatingRowColors(true);
    ui->tableWidgetVehicleBinding->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidgetVehicleBinding->setSortingEnabled(true);
    
    // 设置列宽 - 序号列固定宽度，其他列自适应
    ui->tableWidgetVehicleBinding->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->tableWidgetVehicleBinding->setColumnWidth(0, 60);
    for (int i = 1; i < 4; ++i) {
        ui->tableWidgetVehicleBinding->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
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
    ui->pushButtonVehicleBindingPage->setChecked(false);
}

/**
 * @brief 切换到表格界面
 */
void tcpClient::onTablePageClicked()
{
    ui->stackedWidget->setCurrentIndex(1);
    ui->pushButtonConnectionPage->setChecked(false);
    ui->pushButtonTablePage->setChecked(true);
    ui->pushButtonVehicleBindingPage->setChecked(false);
}

/**
 * @brief 切换到车型绑定界面
 */
void tcpClient::onVehicleBindingPageClicked()
{
    ui->stackedWidget->setCurrentIndex(2);
    ui->pushButtonConnectionPage->setChecked(false);
    ui->pushButtonTablePage->setChecked(false);
    ui->pushButtonVehicleBindingPage->setChecked(true);
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
            clearDataRecords();
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
    // 检查数据长度是否为70字节
    if (data.size() != 70) {
        appendToLog(QString("接收数据长度不正确: %1 字节，期望: 70 字节").arg(data.size()), true);
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
    
    // 解析第5个字节（载荷的第1个字节）
    unsigned char statusByte = static_cast<unsigned char>(data.at(4));
    QString statusDescription;
    unsigned int value1 = 0, value2 = 0;
    
    switch (statusByte) {
        case 0x00:
            statusDescription = "空";
            appendToLog(QString("状态: %1 (0x%2) - 空").arg(statusByte).arg(statusByte, 2, 16, QChar('0')).toUpper(), false);
            break;
        case 0x01:
            statusDescription = "满托送入";
            appendToLog(QString("状态: %1 (0x%2) - 满托送入").arg(statusByte).arg(statusByte, 2, 16, QChar('0')).toUpper(), false);
            break;
        case 0x04:
            statusDescription = "满托送出";
            appendToLog(QString("状态: %1 (0x%2) - 满托送出").arg(statusByte).arg(statusByte, 2, 16, QChar('0')).toUpper(), false);
            break;
        default:
            statusDescription = QString("未知状态(0x%1)").arg(statusByte, 2, 16, QChar('0')).toUpper();
            appendToLog(QString("状态: %1 (0x%2) - 未知状态").arg(statusByte).arg(statusByte, 2, 16, QChar('0')).toUpper(), true);
            break;
    }
    
    // 如果状态不为00，解析第7-8字节和第9-10字节
    if (statusByte != 0x00) {
        // 检查数据长度是否足够
        if (data.size() >= 10) {
            // 解析第7-8字节（高低位颠倒组合）- 滑槽1的车型
            unsigned char byte7 = static_cast<unsigned char>(data.at(6));  // 第7字节
            unsigned char byte8 = static_cast<unsigned char>(data.at(7));  // 第8字节
            value1 = (byte8 << 8) | byte7;  // 高低位颠倒组合
            
            // 解析第9-10字节（高低位颠倒组合）- 滑槽2的车型
            unsigned char byte9 = static_cast<unsigned char>(data.at(8));  // 第9字节
            unsigned char byte10 = static_cast<unsigned char>(data.at(9)); // 第10字节
            value2 = (byte10 << 8) | byte9;  // 高低位颠倒组合
            
            appendToLog(QString("滑槽1车型: 0x%1%2 (DEC: %3)").arg(byte8, 2, 16, QChar('0')).arg(byte7, 2, 16, QChar('0')).arg(value1).toUpper(), false);
            appendToLog(QString("滑槽2车型: 0x%1%2 (DEC: %3)").arg(byte10, 2, 16, QChar('0')).arg(byte9, 2, 16, QChar('0')).arg(value2).toUpper(), false);
            
            // 打印组合后的值
            appendToLog(QString("解析结果 - 状态: %1, 滑槽1车型: %2, 滑槽2车型: %3").arg(statusDescription).arg(value1, 4, 16, QChar('0')).arg(value2, 4, 16, QChar('0')).toUpper(), false);
        } else {
            appendToLog("数据长度不足，无法解析第7-10字节", true);
        }
    } else {
        appendToLog("状态为空，跳过第7-10字节解析", false);
    }
    
    // 解析第6个字节（载荷的第2个字节）
    unsigned char statusByte2 = static_cast<unsigned char>(data.at(5));
    QString statusDescription2;
    unsigned int value3 = 0, value4 = 0;
    
    switch (statusByte2) {
        case 0x00:
            statusDescription2 = "空";
            appendToLog(QString("状态2: %1 (0x%2) - 空").arg(statusByte2).arg(statusByte2, 2, 16, QChar('0')).toUpper(), false);
            break;
        case 0x01:
            statusDescription2 = "空托送入";
            appendToLog(QString("状态2: %1 (0x%2) - 空托送入").arg(statusByte2).arg(statusByte2, 2, 16, QChar('0')).toUpper(), false);
            break;
        case 0x04:
            statusDescription2 = "空托送出";
            appendToLog(QString("状态2: %1 (0x%2) - 空托送出").arg(statusByte2).arg(statusByte2, 2, 16, QChar('0')).toUpper(), false);
            break;
        default:
            statusDescription2 = QString("未知状态(0x%1)").arg(statusByte2, 2, 16, QChar('0')).toUpper();
            appendToLog(QString("状态2: %1 (0x%2) - 未知状态").arg(statusByte2).arg(statusByte2, 2, 16, QChar('0')).toUpper(), true);
            break;
    }
    
    // 如果状态2不为00，解析第39-40字节和第41-42字节
    if (statusByte2 != 0x00) {
        // 检查数据长度是否足够
        if (data.size() >= 42) {
            // 解析第39-40字节（高低位颠倒组合）
            unsigned char byte39 = static_cast<unsigned char>(data.at(38)); // 第39字节
            unsigned char byte40 = static_cast<unsigned char>(data.at(39)); // 第40字节
            value3 = (byte40 << 8) | byte39;  // 高低位颠倒组合
            
            // 解析第41-42字节（高低位颠倒组合）
            unsigned char byte41 = static_cast<unsigned char>(data.at(40)); // 第41字节
            unsigned char byte42 = static_cast<unsigned char>(data.at(41)); // 第42字节
            value4 = (byte42 << 8) | byte41;  // 高低位颠倒组合
            
            appendToLog(QString("第39-40字节: 0x%1%2 (DEC: %3)").arg(byte40, 2, 16, QChar('0')).arg(byte39, 2, 16, QChar('0')).arg(value3).toUpper(), false);
            appendToLog(QString("第41-42字节: 0x%1%2 (DEC: %3)").arg(byte42, 2, 16, QChar('0')).arg(byte41, 2, 16, QChar('0')).arg(value4).toUpper(), false);
            
            // 打印组合后的值
            appendToLog(QString("解析结果2 - 状态: %1, 值3: %2, 值4: %3").arg(statusDescription2).arg(value3, 4, 16, QChar('0')).arg(value4, 4, 16, QChar('0')).toUpper(), false);
        } else {
            appendToLog("数据长度不足，无法解析第39-42字节", true);
        }
    } else {
        appendToLog("状态2为空，跳过第39-42字节解析", false);
    }
    
    QDateTime now = QDateTime::currentDateTime();
    bool processStatus1 = false, processStatus2 = false;
    if (statusByte != 0x00) {
        if (m_lastStatus1Time.isNull() || m_lastStatus1Time.secsTo(now) >= 60) {
            processStatus1 = true;
            m_lastStatus1Time = now;
        } else {
            appendToLog("第5字节一分钟内已处理，忽略本次数据");
        }
    }
    if (statusByte2 != 0x00) {
        if (m_lastStatus2Time.isNull() || m_lastStatus2Time.secsTo(now) >= 60) {
            processStatus2 = true;
            m_lastStatus2Time = now;
        } else {
            appendToLog("第6字节一分钟内已处理，忽略本次数据");
        }
    }
    // 只有需要处理的才emit
    if (processStatus1 || processStatus2) {
        QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        emit dataParsed(processStatus1 ? statusByte : 0x00, processStatus1 ? value1 : 0, processStatus1 ? value2 : 0,
                        processStatus2 ? statusByte2 : 0x00, processStatus2 ? value3 : 0, processStatus2 ? value4 : 0, currentTime);
    }
}

/**
 * @brief 处理解析完成的数据
 */
void tcpClient::onDataParsed(int status1, unsigned int value1, unsigned int value2, 
                           int status2, unsigned int value3, unsigned int value4,
                           const QString &currentTime)
{
    addDataToTable(status1, value1, value2, status2, value3, value4, currentTime);
}

/**
 * @brief 添加数据到表格
 */
void tcpClient::addDataToTable(int status1, unsigned int value1, unsigned int value2, 
                              int status2, unsigned int value3, unsigned int value4,
                              const QString &currentTime)
{
    // 状态字符串定义
    QString statusStr1, statusStr2;
    switch (status1) {
        case 0x01: statusStr1 = "满托送入"; break;
        case 0x04: statusStr1 = "满托送出"; break;
        default: statusStr1 = "未知"; break;
    }
    switch (status2) {
        case 0x01: statusStr2 = "空托送入"; break;
        case 0x04: statusStr2 = "空托送出"; break;
        default: statusStr2 = "未知"; break;
    }
    // 车型名称用16进制字符串表示
    QString modelName1 = QString("%1").arg(value1, 4, 16, QChar('0')).toUpper();
    QString modelName2 = QString("%1").arg(value2, 4, 16, QChar('0')).toUpper();
    QString modelName3 = QString("%1").arg(value3, 4, 16, QChar('0')).toUpper();
    QString modelName4 = QString("%1").arg(value4, 4, 16, QChar('0')).toUpper();
    QString vehicleCode1, vehicleName1; int count1 = 0;
    QString vehicleCode2, vehicleName2; int count2 = 0;
    QString vehicleCode3, vehicleName3; int count3 = 0;
    QString vehicleCode4, vehicleName4; int count4 = 0;
    // 滑槽1/2车型查找（第5字节）
    for (int row = 0; row < ui->tableWidgetVehicleBinding->rowCount(); ++row) {
        QString name = ui->tableWidgetVehicleBinding->item(row, 2)->text().toUpper();
        if (modelName1 == name) {
            vehicleCode1 = ui->tableWidgetVehicleBinding->item(row, 1)->text();
            vehicleName1 = name;
            count1 = ui->tableWidgetVehicleBinding->item(row, 3)->text().toInt();
        }
        if (modelName2 == name) {
            vehicleCode2 = ui->tableWidgetVehicleBinding->item(row, 1)->text();
            vehicleName2 = name;
            count2 = ui->tableWidgetVehicleBinding->item(row, 3)->text().toInt();
        }
        if (modelName3 == name) {
            vehicleCode3 = ui->tableWidgetVehicleBinding->item(row, 1)->text();
            vehicleName3 = name;
            count3 = ui->tableWidgetVehicleBinding->item(row, 3)->text().toInt();
        }
        if (modelName4 == name) {
            vehicleCode4 = ui->tableWidgetVehicleBinding->item(row, 1)->text();
            vehicleName4 = name;
            count4 = ui->tableWidgetVehicleBinding->item(row, 3)->text().toInt();
        }
    }
    // 添加第5字节（满托/空托）数据
    if (status1 != 0x00 && !vehicleName1.isEmpty()) {
        int row1 = ui->tableWidget->rowCount();
        ui->tableWidget->insertRow(row1);
        QTableWidgetItem* t0 = new QTableWidgetItem("1");
        t0->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row1, 0, t0);
        QTableWidgetItem* t1 = new QTableWidgetItem(statusStr1);
        t1->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row1, 1, t1);
        QTableWidgetItem* t2 = new QTableWidgetItem(vehicleCode1);
        t2->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row1, 2, t2);
        QTableWidgetItem* t3 = new QTableWidgetItem(vehicleName1);
        t3->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row1, 3, t3);
        QTableWidgetItem* t4 = new QTableWidgetItem(QString::number(count1));
        t4->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row1, 4, t4);
        QTableWidgetItem* t5 = new QTableWidgetItem(currentTime);
        t5->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row1, 5, t5);
        insertDataRecord(1, statusStr1, vehicleName1, vehicleCode1, count1, currentTime);
    }
    if (status1 != 0x00 && !vehicleName2.isEmpty()) {
        int row2 = ui->tableWidget->rowCount();
        ui->tableWidget->insertRow(row2);
        QTableWidgetItem* t6 = new QTableWidgetItem("2");
        t6->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row2, 0, t6);
        QTableWidgetItem* t7 = new QTableWidgetItem(statusStr1);
        t7->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row2, 1, t7);
        QTableWidgetItem* t8 = new QTableWidgetItem(vehicleCode2);
        t8->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row2, 2, t8);
        QTableWidgetItem* t9 = new QTableWidgetItem(vehicleName2);
        t9->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row2, 3, t9);
        QTableWidgetItem* t10 = new QTableWidgetItem(QString::number(count2));
        t10->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row2, 4, t10);
        QTableWidgetItem* t11 = new QTableWidgetItem(currentTime);
        t11->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row2, 5, t11);
        insertDataRecord(2, statusStr1, vehicleName2, vehicleCode2, count2, currentTime);
    }
    // 添加第6字节（满托/空托）数据
    if (status2 != 0x00 && !vehicleName3.isEmpty()) {
        int row1 = ui->tableWidget->rowCount();
        ui->tableWidget->insertRow(row1);
        QTableWidgetItem* t12 = new QTableWidgetItem("1");
        t12->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row1, 0, t12);
        QTableWidgetItem* t13 = new QTableWidgetItem(statusStr2);
        t13->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row1, 1, t13);
        QTableWidgetItem* t14 = new QTableWidgetItem(vehicleCode3);
        t14->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row1, 2, t14);
        QTableWidgetItem* t15 = new QTableWidgetItem(vehicleName3);
        t15->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row1, 3, t15);
        QTableWidgetItem* t16 = new QTableWidgetItem(QString::number(count3));
        t16->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row1, 4, t16);
        QTableWidgetItem* t17 = new QTableWidgetItem(currentTime);
        t17->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row1, 5, t17);
        insertDataRecord(1, statusStr2, vehicleName3, vehicleCode3, count3, currentTime);
    }
    if (status2 != 0x00 && !vehicleName4.isEmpty()) {
        int row2 = ui->tableWidget->rowCount();
        ui->tableWidget->insertRow(row2);
        QTableWidgetItem* t18 = new QTableWidgetItem("2");
        t18->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row2, 0, t18);
        QTableWidgetItem* t19 = new QTableWidgetItem(statusStr2);
        t19->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row2, 1, t19);
        QTableWidgetItem* t20 = new QTableWidgetItem(vehicleCode4);
        t20->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row2, 2, t20);
        QTableWidgetItem* t21 = new QTableWidgetItem(vehicleName4);
        t21->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row2, 3, t21);
        QTableWidgetItem* t22 = new QTableWidgetItem(QString::number(count4));
        t22->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row2, 4, t22);
        QTableWidgetItem* t23 = new QTableWidgetItem(currentTime);
        t23->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row2, 5, t23);
        insertDataRecord(2, statusStr2, vehicleName4, vehicleCode4, count4, currentTime);
    }
    // 记录到日志
    if ((status1 != 0x00 && (!vehicleName1.isEmpty() || !vehicleName2.isEmpty())) ||
        (status2 != 0x00 && (!vehicleName3.isEmpty() || !vehicleName4.isEmpty()))) {
        appendToLog(QString("数据已添加到表格 - 时间: %1").arg(currentTime), false);
    }
}

/**
 * @brief 添加车型按钮点击处理
 */
void tcpClient::onAddVehicleClicked()
{
    QString vehicleCode = ui->lineEditVehicleCode->text().trimmed();
    QString vehicleName = ui->lineEditVehicleName->text().trimmed();
    QString vehicleCount = ui->lineEditVehicleCount->text().trimmed();
    
    // 验证输入
    if (vehicleCode.isEmpty()) {
        appendToLog("错误: 车型代码不能为空", true);
        return;
    }
    
    if (vehicleName.isEmpty()) {
        appendToLog("错误: 车型名称不能为空", true);
        return;
    }
    
    if (vehicleCount.isEmpty()) {
        appendToLog("错误: 数量不能为空", true);
        return;
    }
    
    bool ok;
    int count = vehicleCount.toInt(&ok);
    if (!ok || count <= 0) {
        appendToLog("错误: 数量必须是正整数", true);
        return;
    }
    
    // 检查是否已存在相同的车型代码
    for (int row = 0; row < ui->tableWidgetVehicleBinding->rowCount(); ++row) {
        QString existingCode = ui->tableWidgetVehicleBinding->item(row, 1)->text();
        if (existingCode == vehicleCode) {
            appendToLog("错误: 车型代码已存在", true);
            return;
        }
    }
    
    // 添加到表格
    int row = ui->tableWidgetVehicleBinding->rowCount();
    ui->tableWidgetVehicleBinding->insertRow(row);
    QTableWidgetItem* item0 = new QTableWidgetItem(QString::number(row + 1));
    item0->setTextAlignment(Qt::AlignCenter);
    ui->tableWidgetVehicleBinding->setItem(row, 0, item0);
    QTableWidgetItem* item1 = new QTableWidgetItem(vehicleCode);
    item1->setTextAlignment(Qt::AlignCenter);
    ui->tableWidgetVehicleBinding->setItem(row, 1, item1);
    QTableWidgetItem* item2 = new QTableWidgetItem(vehicleName);
    item2->setTextAlignment(Qt::AlignCenter);
    ui->tableWidgetVehicleBinding->setItem(row, 2, item2);
    QTableWidgetItem* item3 = new QTableWidgetItem(QString::number(count));
    item3->setTextAlignment(Qt::AlignCenter);
    ui->tableWidgetVehicleBinding->setItem(row, 3, item3);
    // 同步插入数据库
    insertModelBinding(vehicleCode, vehicleName, count);
    
    // 清空输入框
    ui->lineEditVehicleCode->clear();
    ui->lineEditVehicleName->clear();
    ui->lineEditVehicleCount->clear();
    
    appendToLog(QString("成功添加车型绑定: 代码=%1, 名称=%2, 数量=%3").arg(vehicleCode, vehicleName, QString::number(count)));
}

/**
 * @brief 删除选中车型按钮点击处理
 */
void tcpClient::onDeleteVehicleClicked()
{
    QList<QTableWidgetItem*> selectedItems = ui->tableWidgetVehicleBinding->selectedItems();
    if (selectedItems.isEmpty()) {
        appendToLog("请先选择要删除的车型", true);
        return;
    }
    
    // 获取选中的行
    QSet<int> selectedRows;
    for (QTableWidgetItem* item : selectedItems) {
        selectedRows.insert(item->row());
    }
    
    // 从后往前删除，避免索引变化
    QList<int> rowsToDelete = selectedRows.values();
    std::sort(rowsToDelete.begin(), rowsToDelete.end(), std::greater<int>());
    
    for (int row : rowsToDelete) {
        QString vehicleCode = ui->tableWidgetVehicleBinding->item(row, 1)->text();
        QString vehicleName = ui->tableWidgetVehicleBinding->item(row, 2)->text();
        ui->tableWidgetVehicleBinding->removeRow(row);
        appendToLog(QString("已删除车型: %1").arg(vehicleName));
        // 同步删除数据库
        deleteModelBinding(vehicleName);
    }
    
    // 重新编号
    for (int row = 0; row < ui->tableWidgetVehicleBinding->rowCount(); ++row) {
        QTableWidgetItem* item0 = new QTableWidgetItem(QString::number(row + 1));
        item0->setTextAlignment(Qt::AlignCenter);
        ui->tableWidgetVehicleBinding->setItem(row, 0, item0);
    }
}

/**
 * @brief 清空车型表格按钮点击处理
 */
void tcpClient::onClearVehicleTableClicked()
{
    if (ui->tableWidgetVehicleBinding->rowCount() == 0) {
        appendToLog("车型表格已经是空的");
        return;
    }
    
    ui->tableWidgetVehicleBinding->setRowCount(0);
    appendToLog("已清空车型绑定表格");
    // 同步清空数据库
    clearModelBindings();
}

/**
 * @brief 导出车型表格按钮点击处理
 */
void tcpClient::onExportVehicleTableClicked()
{
    if (ui->tableWidgetVehicleBinding->rowCount() == 0) {
        appendToLog("车型表格为空，无法导出", true);
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(this, "导出车型绑定数据", 
                                                   "车型绑定数据.csv", "CSV文件 (*.csv)");
    if (fileName.isEmpty()) {
        return;
    }
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        appendToLog("无法创建文件: " + fileName, true);
        return;
    }
    
    QTextStream out(&file);
    
    // 写入表头
    out << "序号,车型代码,车型名称,数量\n";
    
    // 写入数据
    for (int row = 0; row < ui->tableWidgetVehicleBinding->rowCount(); ++row) {
        QStringList rowData;
        for (int col = 0; col < ui->tableWidgetVehicleBinding->columnCount(); ++col) {
            rowData << ui->tableWidgetVehicleBinding->item(row, col)->text();
        }
        out << rowData.join(",") << "\n";
    }
    
    file.close();
    appendToLog("车型绑定数据已导出到: " + fileName);
}

void tcpClient::initDatabase() {
    QString dbPath = QCoreApplication::applicationDirPath() + "/agt_data.db";
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        appendToLog("数据库打开失败: " + db.lastError().text(), true);
        return;
    }
    QSqlQuery query;
    // 数据记录表
    query.exec("CREATE TABLE IF NOT EXISTS data_records ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
               "slot_no INTEGER,"
               "status TEXT,"
               "model_name TEXT,"
               "model_code TEXT,"
               "count INTEGER,"
               "time TEXT)");
    // 车型绑定表
    query.exec("CREATE TABLE IF NOT EXISTS model_bindings ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
               "model_code TEXT,"
               "model_name TEXT,"
               "count INTEGER)");
}

void tcpClient::insertDataRecord(int slotNo, const QString &status, const QString &modelName, const QString &modelCode, int count, const QString &currentTime) {
    QSqlQuery query;
    query.prepare("INSERT INTO data_records (slot_no, status, model_name, model_code, count, time) VALUES (?, ?, ?, ?, ?, ?)");
    query.addBindValue(slotNo);
    query.addBindValue(status);
    query.addBindValue(modelName);
    query.addBindValue(modelCode);
    query.addBindValue(count);
    query.addBindValue(currentTime);
    if (!query.exec()) {
        appendToLog("插入数据记录失败: " + query.lastError().text(), true);
    }
}

void tcpClient::insertModelBinding(const QString &modelCode, const QString &modelName, int count) {
    QSqlQuery query;
    query.prepare("INSERT INTO model_bindings (model_code, model_name, count) VALUES (?, ?, ?)");
    query.addBindValue(modelCode);
    query.addBindValue(modelName);
    query.addBindValue(count);
    if (!query.exec()) {
        appendToLog("插入车型绑定失败: " + query.lastError().text(), true);
    }
}

void tcpClient::deleteModelBinding(const QString &modelName) {
    QSqlQuery query;
    query.prepare("DELETE FROM model_bindings WHERE model_name = ?");
    query.addBindValue(modelName);
    if (!query.exec()) {
        appendToLog("删除车型绑定失败: " + query.lastError().text(), true);
    }
}

void tcpClient::clearModelBindings() {
    QSqlQuery query;
    if (!query.exec("DELETE FROM model_bindings")) {
        appendToLog("清空车型绑定表失败: " + query.lastError().text(), true);
    }
}

void tcpClient::clearDataRecords() {
    QSqlQuery query;
    if (!query.exec("DELETE FROM data_records")) {
        appendToLog("清空数据记录表失败: " + query.lastError().text(), true);
    }
}

void tcpClient::loadModelBindingsFromDb() {
    ui->tableWidgetVehicleBinding->setRowCount(0);
    QSqlQuery query("SELECT model_code, model_name, count FROM model_bindings");
    int row = 0;
    while (query.next()) {
        ui->tableWidgetVehicleBinding->insertRow(row);
        QTableWidgetItem* item0 = new QTableWidgetItem(QString::number(row + 1));
        item0->setTextAlignment(Qt::AlignCenter);
        ui->tableWidgetVehicleBinding->setItem(row, 0, item0);
        QTableWidgetItem* item1 = new QTableWidgetItem(query.value(0).toString());
        item1->setTextAlignment(Qt::AlignCenter);
        ui->tableWidgetVehicleBinding->setItem(row, 1, item1);
        QTableWidgetItem* item2 = new QTableWidgetItem(query.value(1).toString());
        item2->setTextAlignment(Qt::AlignCenter);
        ui->tableWidgetVehicleBinding->setItem(row, 2, item2);
        QTableWidgetItem* item3 = new QTableWidgetItem(query.value(2).toString());
        item3->setTextAlignment(Qt::AlignCenter);
        ui->tableWidgetVehicleBinding->setItem(row, 3, item3);
        ++row;
    }
}

void tcpClient::loadDataRecordsFromDb() {
    ui->tableWidget->setRowCount(0);
    QSqlQuery query("SELECT slot_no, status, model_code, model_name, count, time FROM data_records");
    int row = 0;
    while (query.next()) {
        ui->tableWidget->insertRow(row);
        QTableWidgetItem* t0 = new QTableWidgetItem(query.value(0).toInt() == 1 ? "1" : "2");
        t0->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row, 0, t0);
        QTableWidgetItem* t1 = new QTableWidgetItem(query.value(1).toString());
        t1->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row, 1, t1);
        QTableWidgetItem* t2 = new QTableWidgetItem(query.value(2).toString());
        t2->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row, 2, t2);
        QTableWidgetItem* t3 = new QTableWidgetItem(query.value(3).toString());
        t3->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row, 3, t3);
        QTableWidgetItem* t4 = new QTableWidgetItem(query.value(4).toString());
        t4->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row, 4, t4);
        QTableWidgetItem* t5 = new QTableWidgetItem(query.value(5).toString());
        t5->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row, 5, t5);
        ++row;
    }
}
