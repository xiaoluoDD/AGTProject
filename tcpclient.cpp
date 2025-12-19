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
#include <QScreen>
#include <QGuiApplication>
#include <QFile>
#include <QTimer>
#include <QDebug>
#include <QInputDialog>
#include <QCloseEvent>
#include <QLoggingCategory>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QSettings>

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
    , m_password("")
    , m_isPasswordSet(false)
    , m_dbHost("localhost")
    , m_dbPort(3306)
    , m_dbName("agt_database")
    , m_dbUsername("agt_user1")
    , m_dbPassword("root123.")
    , m_logFile(nullptr)
    , m_logStream(nullptr)
{
    // 设置静态实例指针
    s_instance = this;

    // 添加插件路径，确保能找到MySQL驱动
    QString appDir = QCoreApplication::applicationDirPath();
    QString pluginPath = appDir + "/plugins/sqldrivers";
    QString pluginPath2 = appDir;  // 也检查exe同目录
    QCoreApplication::addLibraryPath(pluginPath);
    QCoreApplication::addLibraryPath(pluginPath2);
    qDebug() << "应用程序目录:" << appDir;
    qDebug() << "插件路径1:" << pluginPath;
    qDebug() << "插件路径2:" << pluginPath2;

    qDebug() << "tcpClient constructor started";

    try {
        ui->setupUi(this);
        qDebug() << "UI setup completed";

        // 初始化日志系统（在其他初始化之前）
        initLogSystem();
        qDebug() << "initLogSystem completed";

        setupUI();
        qDebug() << "setupUI completed";

        setupTable();
        qDebug() << "setupTable completed";

        setupVehicleBindingTable();
        qDebug() << "setupVehicleBindingTable completed";

        initDatabase();
        qDebug() << "initDatabase completed";

        initPasswordTable();
        qDebug() << "initPasswordTable completed";

        loadPassword();
        qDebug() << "loadPassword completed";

        // 延迟更新密码显示，确保UI已准备好
        QTimer::singleShot(100, this, &tcpClient::updatePasswordDisplay);

        loadModelBindingsFromDb();
        qDebug() << "loadModelBindingsFromDb completed";

        loadDataRecordsFromDb();
        qDebug() << "loadDataRecordsFromDb completed";

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

        // 在UI完全初始化后记录启动日志
        appendToLog("TCP客户端已启动");
        qInfo() << "AGT滑槽记录表客户端启动完成";

        qDebug() << "tcpClient constructor completed successfully";
    } catch (const std::exception& e) {
        qDebug() << "Exception in constructor:" << e.what();
    } catch (...) {
        qDebug() << "Unknown exception in constructor";
    }
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

    // 清理日志系统资源
    if (m_logStream) {
        delete m_logStream;
        m_logStream = nullptr;
    }
    if (m_logFile) {
        m_logFile->close();
        delete m_logFile;
        m_logFile = nullptr;
    }

    // 清除静态实例指针
    s_instance = nullptr;

    delete ui;
}

/**
 * @brief 初始化用户界面
 */
void tcpClient::setupUI()
{
    setWindowTitle("AGT滑槽记录表");

    // 设置默认连接参数
    ui->lineEditServerIP->setText("142.2.21.100");
    ui->lineEditPort->setText("5001");

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
    connect(ui->pushButtonSetPassword, &QPushButton::clicked, this, &tcpClient::onSetPasswordClicked);
    
    // 连接数据库配置按钮信号槽
    connect(ui->pushButtonSaveDbConfig, &QPushButton::clicked, this, &tcpClient::onSaveDatabaseConfigClicked);
    connect(ui->pushButtonTestDbConnection, &QPushButton::clicked, this, &tcpClient::onTestDatabaseConnectionClicked);
    
    // 加载数据库配置到界面
    loadDatabaseConfig();

    // 连接表格操作按钮信号槽
    connect(ui->pushButtonClearTable, &QPushButton::clicked, this, &tcpClient::onClearTableClicked);
    connect(ui->pushButtonDeleteTable, &QPushButton::clicked, this, &tcpClient::onDeleteTableClicked);
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

    // 设置初始界面为数据表格界面
    ui->stackedWidget->setCurrentIndex(1);
    ui->pushButtonConnectionPage->setChecked(false);
    ui->pushButtonTablePage->setChecked(true);
    ui->pushButtonVehicleBindingPage->setChecked(false);

    ui->pushButtonExportTable->setVisible(false);
    // 隐藏上方状态标签
    ui->labelStatus->setVisible(false);
    // 左下角连接状态标签
    labelConnectionStatus = new QLabel(this);
    labelConnectionStatus->setText("未连接");
    labelConnectionStatus->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    // 浣跨敤Qt榛樿鏍峰紡`r`n    labelConnectionStatus->setStyleSheet("");
    ui->verticalLayout->addWidget(labelConnectionStatus);

    // 应用现代化样式
    applyModernStyle();
}

/**
 * @brief 应用现代化样式
 */
void tcpClient::applyModernStyle()
{
    // 尝试从外部文件加载样式
    QFile styleFile("style.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QString styleSheet = QLatin1String(styleFile.readAll());
        this->setStyleSheet(styleSheet);
        styleFile.close();
        // 延迟记录日志，确保UI已准备好
        QTimer::singleShot(100, this, [this]() {
            appendToLog("已从外部文件加载样式表");
        });
    } else {
        // 如果外部文件不存在，使用内置样式
        // 宸茬鐢ㄥ唴缃牱寮忥紝浣跨敤Qt榛樿鏍峰紡
        // applyBuiltinStyle();
        this->setStyleSheet("");

        // 鏄惧紡閲嶇疆鎵€鏈夋寜閽殑鏍峰紡
        if (ui) {
            ui->pushButtonConnectionPage->setStyleSheet("");
            ui->pushButtonTablePage->setStyleSheet("");
            ui->pushButtonVehicleBindingPage->setStyleSheet("");
            ui->pushButtonClearTable->setStyleSheet("");
            ui->pushButtonDeleteTable->setStyleSheet("");
            ui->pushButtonConnect->setStyleSheet("");
            ui->pushButtonDisconnect->setStyleSheet("");
        }

        // 寤惰繜璁板綍鏃ュ織
        QTimer::singleShot(100, this, [this]() {
            appendToLog("宸叉竻绌鸿嚜瀹氫箟鏍峰紡锛屼娇鐢≦t榛樿鏍峰紡");
        });

    }
}
void tcpClient::applyBuiltinStyle()
{
    // 主窗口样式
    QString mainWindowStyle = R"(
        QWidget {
            font-family: 'Microsoft YaHei', 'Segoe UI', Arial, sans-serif;
            font-size: 9pt;
            background-color: #f5f5f5;
        }

        QGroupBox {
            font-weight: bold;
            border: 2px solid #d0d0d0;
            border-radius: 8px;
            margin-top: 10px;
            padding-top: 10px;
            background-color: white;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 8px 0 8px;
            color: #2c3e50;
            background-color: white;
        }
    )";

    // 按钮样式
    QString buttonStyle = R"(
        QPushButton {
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                      stop: 0 #3498db, stop: 1 #2980b9);
            border: none;
            color: white;
            padding: 8px 16px;
            border-radius: 6px;
            font-weight: bold;
            min-height: 20px;
        }

        QPushButton:hover {
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                      stop: 0 #5dade2, stop: 1 #3498db);
        }

        QPushButton:pressed {
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                      stop: 0 #2980b9, stop: 1 #21618c);
        }

        QPushButton:disabled {
            background: #bdc3c7;
            color: #7f8c8d;
        }
    )";

    // 连接按钮特殊样式
    QString connectButtonStyle = R"(
        QPushButton#pushButtonConnect {
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                      stop: 0 #27ae60, stop: 1 #229954);
        }

        QPushButton#pushButtonConnect:hover {
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                      stop: 0 #58d68d, stop: 1 #27ae60);
        }

        QPushButton#pushButtonDisconnect {
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                      stop: 0 #e74c3c, stop: 1 #c0392b);
        }

        QPushButton#pushButtonDisconnect:hover {
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                      stop: 0 #ec7063, stop: 1 #e74c3c);
        }
    )";

    // 危险操作按钮样式
    QString dangerButtonStyle = R"(
        QPushButton#pushButtonClearTable,
        QPushButton#pushButtonDeleteTable,
        QPushButton#pushButtonClearVehicleTable,
        QPushButton#pushButtonDeleteVehicle {
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                      stop: 0 #e74c3c, stop: 1 #c0392b);
        }

        QPushButton#pushButtonClearTable:hover,
        QPushButton#pushButtonDeleteTable:hover,
        QPushButton#pushButtonClearVehicleTable:hover,
        QPushButton#pushButtonDeleteVehicle:hover {
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                      stop: 0 #ec7063, stop: 1 #e74c3c);
        }
    )";

    // 导航按钮样式
    QString navButtonStyle = R"(
        QPushButton#pushButtonConnectionPage,
        QPushButton#pushButtonTablePage,
        QPushButton#pushButtonVehicleBindingPage {
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                      stop: 0 #34495e, stop: 1 #2c3e50);
            border: none;
            color: white;
            padding: 10px 20px;
            border-radius: 6px;
            font-weight: bold;
            min-height: 25px;
        }

        QPushButton#pushButtonConnectionPage:hover,
        QPushButton#pushButtonTablePage:hover,
        QPushButton#pushButtonVehicleBindingPage:hover {
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                      stop: 0 #5d6d7e, stop: 1 #34495e);
        }

        QPushButton#pushButtonConnectionPage:checked,
        QPushButton#pushButtonTablePage:checked,
        QPushButton#pushButtonVehicleBindingPage:checked {
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                      stop: 0 #3498db, stop: 1 #2980b9);
            border: 2px solid #2980b9;
        }
    )";

    // 输入框样式
    QString inputStyle = R"(
        QLineEdit, QTextEdit {
            border: 2px solid #bdc3c7;
            border-radius: 6px;
            padding: 6px;
            background-color: white;
            selection-background-color: #3498db;
        }

        QLineEdit:focus, QTextEdit:focus {
            border-color: #3498db;
            outline: none;
        }

        QLineEdit:hover, QTextEdit:hover {
            border-color: #95a5a6;
        }
    )";

    // 表格样式
    QString tableStyle = R"(
        QTableWidget {
            background-color: white;
            alternate-background-color: #f8f9fa;
            gridline-color: #dee2e6;
            border: 1px solid #dee2e6;
            border-radius: 6px;
            selection-background-color: #3498db;
            selection-color: white;
        }

        QTableWidget::item {
            padding: 8px;
            border-bottom: 1px solid #dee2e6;
        }

        QTableWidget::item:selected {
            background-color: #3498db;
            color: white;
        }

        QHeaderView::section {
            background-color: #34495e;
            color: white;
            padding: 10px;
            border: none;
            font-weight: bold;
        }

        QHeaderView::section:hover {
            background-color: #5d6d7e;
        }
    )";

    // 状态标签样式
    QString statusStyle = R"(
        QLabel#labelConnectionStatus {
            padding: 8px 12px;
            border-radius: 6px;
            font-weight: bold;
            font-size: 10pt;
        }
    )";

    // 组合所有样式
    QString completeStyle = mainWindowStyle + buttonStyle + connectButtonStyle +
                            dangerButtonStyle + navButtonStyle + inputStyle +
                            tableStyle + statusStyle;

    // 应用样式
    this->setStyleSheet(completeStyle);
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

    // 设置表格为只读模式，防止用户修改数据
    ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // 设置列宽 - 优化各列宽度分配
    // 滑槽号 - 固定宽度，内容较短
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->tableWidget->setColumnWidth(0, 80);

    // 送入送出状态 - 固定宽度，内容固定
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    ui->tableWidget->setColumnWidth(1, 120);

    // 车型代码 - 固定宽度，内容较短
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    ui->tableWidget->setColumnWidth(2, 100);

    // 车型名称 - 固定宽度，内容较短
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    ui->tableWidget->setColumnWidth(3, 120);

    // 数量（件） - 固定宽度，内容较短
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    ui->tableWidget->setColumnWidth(4, 100);

    // 时间 - 自适应宽度，确保完整显示
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);

    // 设置表格最小宽度，确保时间列有足够空间
    ui->tableWidget->setMinimumWidth(700);

    // 延迟记录列宽设置到日志，确保UI已准备好
    QTimer::singleShot(200, this, [this]() {
        appendToLog("数据表格列宽设置完成：滑槽号(80px), 状态(120px), 代码(100px), 名称(120px), 数量(100px), 时间(自适应)");
        appendToLog("数据表格已设置为只读模式，防止数据被误修改");
    });
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

    // 设置车型绑定表格为不可编辑
    ui->tableWidgetVehicleBinding->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // 设置列宽 - 序号列固定宽度，其他列自适应
    ui->tableWidgetVehicleBinding->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->tableWidgetVehicleBinding->setColumnWidth(0, 60);
    for (int i = 1; i < 4; ++i) {
        ui->tableWidgetVehicleBinding->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
    }

    // 连接数据变化信号，当用户编辑车型信息时自动保存
    connect(ui->tableWidgetVehicleBinding, &QTableWidget::itemChanged,
            this, &tcpClient::onVehicleBindingItemChanged);
}

/**
 * @brief 切换到连接界面
 */
void tcpClient::onConnectionPageClicked()
{
    // 检查是否需要密码验证
    if (m_isPasswordSet) {
        if (!showPasswordDialog("密码验证", "请输入系统设置密码:")) {
            // 密码验证失败或取消，恢复按钮状态
            ui->pushButtonConnectionPage->setChecked(false);
            ui->pushButtonTablePage->setChecked(true);
            return;
        }
    }

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
 * @brief 删除选中表格按钮点击处理
 */
void tcpClient::onDeleteTableClicked()
{
    QList<QTableWidgetItem*> selectedItems = ui->tableWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        appendToLog("请先选择要删除的记录", true);
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

    // 确认删除
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认删除",
        QString("确定要删除选中的 %1 条记录吗？").arg(rowsToDelete.size()),
        QMessageBox::Yes | QMessageBox::No
        );

    if (reply == QMessageBox::Yes) {
        for (int row : rowsToDelete) {
            // 获取要删除的记录信息用于数据库删除
            QString slotNo = ui->tableWidget->item(row, 0)->text();
            QString status = ui->tableWidget->item(row, 1)->text();
            QString modelCode = ui->tableWidget->item(row, 2)->text();
            QString modelName = ui->tableWidget->item(row, 3)->text();
            QString count = ui->tableWidget->item(row, 4)->text();
            QString time = ui->tableWidget->item(row, 5)->text();

            // 从表格中删除
            ui->tableWidget->removeRow(row);

            // 从数据库中删除对应记录
            deleteDataRecord(slotNo.toInt(), status, modelName, modelCode, count.toInt(), time);
        }

        appendToLog(QString("已删除 %1 条记录").arg(rowsToDelete.size()));
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

    qInfo() << "用户尝试连接服务器:" << serverIP << ":" << portStr;

    // 验证IP地址
    if (serverIP.isEmpty()) {
        QString errorMsg = "请输入服务器IP地址";
        QMessageBox::warning(this, "警告", errorMsg);
        qWarning() << errorMsg;
        return;
    }

    // 验证端口号
    bool ok;
    int port = portStr.toInt(&ok);
    if (!ok || port <= 0 || port > 65535) {
        QString errorMsg = "请输入有效的端口号(1-65535)";
        QMessageBox::warning(this, "警告", errorMsg);
        qWarning() << errorMsg << "输入值:" << portStr;
        return;
    }

    // 开始连接
    QString connectMsg = QString("正在连接到 %1:%2...").arg(serverIP).arg(port);
    appendToLog(connectMsg);
    qInfo() << connectMsg;

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

    QString successMsg = "连接成功！";
    appendToLog(successMsg, false);
    qInfo() << successMsg << "服务器地址:" << m_socket->peerAddress().toString() << "端口:" << m_socket->peerPort();
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
        labelConnectionStatus->setText("已连接");
        // 浣跨敤Qt榛樿鏍峰紡`r`n        labelConnectionStatus->setStyleSheet("");
        ui->pushButtonConnect->setEnabled(false);
        ui->pushButtonDisconnect->setEnabled(true);
        ui->lineEditServerIP->setEnabled(false);
        ui->lineEditPort->setEnabled(false);
    } else {
        labelConnectionStatus->setText("未连接");
        // 浣跨敤Qt榛樿鏍峰紡`r`n        labelConnectionStatus->setStyleSheet("");
        ui->pushButtonConnect->setEnabled(true);
        ui->pushButtonDisconnect->setEnabled(false);
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
    // 安全检查：确保UI组件已初始化
    if (!ui || !ui->textEditLog) {
        qDebug() << "UI not ready, skipping log:" << message;
        return;
    }

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

    // 同时写入文件日志
    if (isError) {
        qWarning() << message;
    } else {
        qInfo() << message;
    }
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
    qDebug() << "接收到数据，长度:" << data.size() << "字节";

    // 检查数据长度是否为78字节
    if (data.size() != 78) {
        QString errorMsg = QString("接收数据长度不正确: %1 字节，期望: 78 字节").arg(data.size());
        appendToLog(errorMsg, true);
        qWarning() << errorMsg;
        return;
    }

    // 检查前4个字节是否为固定的 60 00 4A 00
    QByteArray header = data.left(4);
    QByteArray expectedHeader = QByteArray::fromHex("60004A00");

    if (header != expectedHeader) {
        QString errorMsg = QString("接收数据头部不匹配，期望: 60 00 4A 00，实际: %1")
                               .arg(header.toHex(' ').toUpper());
        appendToLog(errorMsg, true);
        qWarning() << errorMsg;
        return;
    }

    qDebug() << "数据格式验证通过，开始解析数据";

    // 定义托盘信息结构
    struct TrayInfo {
        int slotNo;           // 托盘号（1-6）
        unsigned char status; // 状态字节
        unsigned int modelCode; // 车型代码
        QString statusStr;    // 状态字符串
        bool isRealTray;      // 是否为实托盘
    };

    QList<TrayInfo> trayList;
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

    // 解析实托盘状态（第5、6、7字节）
    for (int i = 0; i < 3; ++i) {
        unsigned char statusByte = static_cast<unsigned char>(data.at(4 + i)); // 第5、6、7字节
        TrayInfo tray;
        tray.slotNo = i + 1;
        tray.status = statusByte;
        tray.isRealTray = true;
        tray.modelCode = 0;

        if (statusByte == 0x00) {
            tray.statusStr = "空";
            appendToLog(QString("实托盘%1: 空 (0x%2)").arg(tray.slotNo).arg(statusByte, 2, 16, QChar('0')).toUpper(), false);
        } else if (statusByte == 0x01) {
            tray.statusStr = "实托盘搬入";
            // 解析车型代码：第15、16字节对应第5字节，第17、18字节对应第6字节，第19、20字节对应第7字节
            if (data.size() >= 20) {
                unsigned char byteLow = static_cast<unsigned char>(data.at(14 + i * 2));   // 第15、17、19字节
                unsigned char byteHigh = static_cast<unsigned char>(data.at(15 + i * 2));  // 第16、18、20字节
                tray.modelCode = (byteHigh << 8) | byteLow;  // 高低位颠倒组合
                appendToLog(QString("实托盘%1: 搬入，车型代码: 0x%2%3 (DEC: %4)")
                           .arg(tray.slotNo)
                           .arg(byteHigh, 2, 16, QChar('0'))
                           .arg(byteLow, 2, 16, QChar('0'))
                           .arg(tray.modelCode).toUpper(), false);
            }
        } else if (statusByte == 0x02) {
            tray.statusStr = "实托盘搬出";
            // 解析车型代码
            if (data.size() >= 20) {
                unsigned char byteLow = static_cast<unsigned char>(data.at(14 + i * 2));   // 第15、17、19字节
                unsigned char byteHigh = static_cast<unsigned char>(data.at(15 + i * 2));  // 第16、18、20字节
                tray.modelCode = (byteHigh << 8) | byteLow;  // 高低位颠倒组合
                appendToLog(QString("实托盘%1: 搬出，车型代码: 0x%2%3 (DEC: %4)")
                           .arg(tray.slotNo)
                           .arg(byteHigh, 2, 16, QChar('0'))
                           .arg(byteLow, 2, 16, QChar('0'))
                           .arg(tray.modelCode).toUpper(), false);
            }
        } else {
            tray.statusStr = QString("未知状态(0x%1)").arg(statusByte, 2, 16, QChar('0')).toUpper();
            appendToLog(QString("实托盘%1: 未知状态 (0x%2)").arg(tray.slotNo).arg(statusByte, 2, 16, QChar('0')).toUpper(), true);
        }

        if (statusByte != 0x00) {
            trayList.append(tray);
        }
    }

    // 解析空托盘状态（第8、9、10字节）
    for (int i = 0; i < 3; ++i) {
        unsigned char statusByte = static_cast<unsigned char>(data.at(7 + i)); // 第8、9、10字节
        TrayInfo tray;
        tray.slotNo = i + 4; // 空托盘编号为4、5、6
        tray.status = statusByte;
        tray.isRealTray = false;
        tray.modelCode = 0;

        if (statusByte == 0x00) {
            tray.statusStr = "空";
            appendToLog(QString("空托盘%1: 空 (0x%2)").arg(tray.slotNo).arg(statusByte, 2, 16, QChar('0')).toUpper(), false);
        } else if (statusByte == 0x01) {
            tray.statusStr = "空托盘搬入";
            // 解析车型代码：第21、22字节对应第8字节，第23、24字节对应第9字节，第25、26字节对应第10字节
            if (data.size() >= 26) {
                unsigned char byteLow = static_cast<unsigned char>(data.at(20 + i * 2));   // 第21、23、25字节
                unsigned char byteHigh = static_cast<unsigned char>(data.at(21 + i * 2)); // 第22、24、26字节
                tray.modelCode = (byteHigh << 8) | byteLow;  // 高低位颠倒组合
                appendToLog(QString("空托盘%1: 搬入，车型代码: 0x%2%3 (DEC: %4)")
                           .arg(tray.slotNo)
                           .arg(byteHigh, 2, 16, QChar('0'))
                           .arg(byteLow, 2, 16, QChar('0'))
                           .arg(tray.modelCode).toUpper(), false);
            }
        } else if (statusByte == 0x02) {
            tray.statusStr = "空托盘搬出";
            // 解析车型代码
            if (data.size() >= 26) {
                unsigned char byteLow = static_cast<unsigned char>(data.at(20 + i * 2));   // 第21、23、25字节
                unsigned char byteHigh = static_cast<unsigned char>(data.at(21 + i * 2)); // 第22、24、26字节
                tray.modelCode = (byteHigh << 8) | byteLow;  // 高低位颠倒组合
                appendToLog(QString("空托盘%1: 搬出，车型代码: 0x%2%3 (DEC: %4)")
                           .arg(tray.slotNo)
                           .arg(byteHigh, 2, 16, QChar('0'))
                           .arg(byteLow, 2, 16, QChar('0'))
                           .arg(tray.modelCode).toUpper(), false);
            }
        } else {
            tray.statusStr = QString("未知状态(0x%1)").arg(statusByte, 2, 16, QChar('0')).toUpper();
            appendToLog(QString("空托盘%1: 未知状态 (0x%2)").arg(tray.slotNo).arg(statusByte, 2, 16, QChar('0')).toUpper(), true);
        }

        if (statusByte != 0x00) {
            trayList.append(tray);
        }
    }

    // 防抖处理：检查5秒内是否已处理
    QDateTime now = QDateTime::currentDateTime();
    QList<TrayInfo> processedTrays;

    for (const TrayInfo& tray : trayList) {
        bool shouldProcess = false;
        QString timeKey = QString("%1_%2").arg(tray.isRealTray ? "real" : "empty").arg(tray.slotNo);
        
        // 检查该托盘是否在5秒内已处理
        if (!m_lastTrayTime.contains(timeKey) || 
            m_lastTrayTime[timeKey].secsTo(now) >= 5) {
            shouldProcess = true;
            m_lastTrayTime[timeKey] = now;
        } else {
            appendToLog(QString("托盘%1在5秒内已处理，忽略本次数据").arg(tray.slotNo), false);
        }

        if (shouldProcess) {
            processedTrays.append(tray);
        }
    }

    // 处理所有需要处理的托盘数据
    if (!processedTrays.isEmpty()) {
        for (const TrayInfo& tray : processedTrays) {
            // 查找车型信息：将16进制的车型代码转换为10进制字符串进行匹配
            // tray.modelCode 已经是10进制的unsigned int值，直接转换为10进制字符串
            QString modelCodeStr = QString::number(tray.modelCode);
            QString vehicleCode, vehicleName;
            int count = 0;

            for (int row = 0; row < ui->tableWidgetVehicleBinding->rowCount(); ++row) {
                // 绑定表中的车型代码是10进制格式，直接比较
                QString code = ui->tableWidgetVehicleBinding->item(row, 1)->text().trimmed();
                if (modelCodeStr == code) {
                    vehicleCode = code;
                    vehicleName = ui->tableWidgetVehicleBinding->item(row, 2)->text();
                    count = ui->tableWidgetVehicleBinding->item(row, 3)->text().toInt();
                    break;
                }
            }

            // 如果找到车型信息，添加到表格
            if (!vehicleName.isEmpty()) {
                int tableRow = ui->tableWidget->rowCount();
                ui->tableWidget->insertRow(tableRow);
                
                // 滑槽号（实托盘1-3显示为1-3，空托盘1-3显示为4-6）
                QString slotNoStr = tray.isRealTray ? QString::number(tray.slotNo) : QString::number(tray.slotNo);
                QTableWidgetItem* item0 = new QTableWidgetItem(slotNoStr);
                item0->setTextAlignment(Qt::AlignCenter);
                ui->tableWidget->setItem(tableRow, 0, item0);
                
                // 状态
                QTableWidgetItem* item1 = new QTableWidgetItem(tray.statusStr);
                item1->setTextAlignment(Qt::AlignCenter);
                ui->tableWidget->setItem(tableRow, 1, item1);
                
                // 车型代码
                QTableWidgetItem* item2 = new QTableWidgetItem(vehicleCode);
                item2->setTextAlignment(Qt::AlignCenter);
                ui->tableWidget->setItem(tableRow, 2, item2);
                
                // 车型名称
                QTableWidgetItem* item3 = new QTableWidgetItem(vehicleName);
                item3->setTextAlignment(Qt::AlignCenter);
                ui->tableWidget->setItem(tableRow, 3, item3);
                
                // 数量
                QTableWidgetItem* item4 = new QTableWidgetItem(QString::number(count));
                item4->setTextAlignment(Qt::AlignCenter);
                ui->tableWidget->setItem(tableRow, 4, item4);
                
                // 时间
                QTableWidgetItem* item5 = new QTableWidgetItem(currentTime);
                item5->setTextAlignment(Qt::AlignCenter);
                ui->tableWidget->setItem(tableRow, 5, item5);
                
                // 保存到数据库
                insertDataRecord(slotNoStr.toInt(), tray.statusStr, vehicleName, vehicleCode, count, currentTime);
                
                appendToLog(QString("托盘%1数据已添加到表格 - %2, 车型: %3").arg(slotNoStr).arg(tray.statusStr).arg(vehicleName), false);
            } else if (tray.modelCode != 0) {
                appendToLog(QString("托盘%1的车型代码 %2 未在绑定表中找到").arg(tray.slotNo).arg(modelCodeStr), true);
            }
        }
    }

    // 收到正确数据后立即返回E0 00
    QByteArray ack;
    ack.append(static_cast<char>(0xE0));
    ack.append(static_cast<char>(0x00));
    m_socket->write(ack);
    appendToLog("已发送应答: E0 00", false);
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
    // 车型代码用10进制字符串表示（绑定表中的车型代码是10进制格式）
    QString modelCode1 = QString::number(value1);
    QString modelCode2 = QString::number(value2);
    QString modelCode3 = QString::number(value3);
    QString modelCode4 = QString::number(value4);
    QString vehicleCode1, vehicleName1; int count1 = 0;
    QString vehicleCode2, vehicleName2; int count2 = 0;
    QString vehicleCode3, vehicleName3; int count3 = 0;
    QString vehicleCode4, vehicleName4; int count4 = 0;
    // 滑槽1/2车型查找（第5字节）- 在车型代码列（列1）中查找
    for (int row = 0; row < ui->tableWidgetVehicleBinding->rowCount(); ++row) {
        QString code = ui->tableWidgetVehicleBinding->item(row, 1)->text().trimmed(); // 车型代码列（10进制格式）
        if (modelCode1 == code) {
            vehicleCode1 = code; // 车型代码
            vehicleName1 = ui->tableWidgetVehicleBinding->item(row, 2)->text(); // 车型名称
            count1 = ui->tableWidgetVehicleBinding->item(row, 3)->text().toInt();
        }
        if (modelCode2 == code) {
            vehicleCode2 = code; // 车型代码
            vehicleName2 = ui->tableWidgetVehicleBinding->item(row, 2)->text(); // 车型名称
            count2 = ui->tableWidgetVehicleBinding->item(row, 3)->text().toInt();
        }
        if (modelCode3 == code) {
            vehicleCode3 = code; // 车型代码
            vehicleName3 = ui->tableWidgetVehicleBinding->item(row, 2)->text(); // 车型名称
            count3 = ui->tableWidgetVehicleBinding->item(row, 3)->text().toInt();
        }
        if (modelCode4 == code) {
            vehicleCode4 = code; // 车型代码
            vehicleName4 = ui->tableWidgetVehicleBinding->item(row, 2)->text(); // 车型名称
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

/**
 * @brief 车型绑定表格数据变化处理
 * @param item 变化的表格项
 */
void tcpClient::onVehicleBindingItemChanged(QTableWidgetItem *item)
{
    if (!item) return;

    // 安全检查：确保UI组件已初始化
    if (!ui || !ui->tableWidgetVehicleBinding) {
        qDebug() << "UI not ready for onVehicleBindingItemChanged";
        return;
    }

    int row = item->row();
    int col = item->column();
    QString newValue = item->text();

    // 获取该行的完整信息，添加安全检查
    QTableWidgetItem* codeItem = ui->tableWidgetVehicleBinding->item(row, 1);
    QTableWidgetItem* nameItem = ui->tableWidgetVehicleBinding->item(row, 2);
    QTableWidgetItem* countItem = ui->tableWidgetVehicleBinding->item(row, 3);

    if (!codeItem || !nameItem || !countItem) {
        qDebug() << "Table items not found for row:" << row;
        return;
    }

    QString vehicleCode = codeItem->text();
    QString vehicleName = nameItem->text();
    QString vehicleCount = countItem->text();

    // 验证输入
    if (col == 3) { // 数量列
        bool ok;
        int count = newValue.toInt(&ok);
        if (!ok || count <= 0) {
            appendToLog("错误: 数量必须是正整数", true);
            // 恢复原值
            item->setText(vehicleCount);
            return;
        }
    }

    if (col == 1 && newValue.isEmpty()) { // 车型代码列
        appendToLog("错误: 车型代码不能为空", true);
        item->setText(vehicleCode);
        return;
    }

    if (col == 2 && newValue.isEmpty()) { // 车型名称列
        appendToLog("错误: 车型名称不能为空", true);
        item->setText(vehicleName);
        return;
    }

    // 更新数据库中的对应记录
    updateModelBinding(vehicleCode, vehicleName, newValue.toInt(), row);

    appendToLog(QString("已更新车型绑定信息: 行%1, 列%2, 新值: %3").arg(row + 1).arg(col + 1).arg(newValue));
}

void tcpClient::initDatabase() {
    qInfo() << "初始化MySQL数据库连接";

    // 检查可用的数据库驱动
    QStringList drivers = QSqlDatabase::drivers();
    qDebug() << "可用的数据库驱动:" << drivers;
    appendToLog(QString("可用的数据库驱动: %1").arg(drivers.join(", ")));

    // 检查QMYSQL驱动是否可用
    if (!drivers.contains("QMYSQL")) {
        QString errorMsg = "错误: QMYSQL驱动不可用！请确保qsqlmysql.dll在plugins/sqldrivers目录或exe同目录下。";
        appendToLog(errorMsg, true);
        qCritical() << errorMsg;
        qCritical() << "当前可用的驱动:" << drivers;
        QMessageBox::critical(nullptr, "数据库驱动错误",
                              QString("QMYSQL驱动不可用！\n\n"
                                      "请确保:\n"
                                      "1. qsqlmysql.dll 在 plugins/sqldrivers 目录下\n"
                                      "2. libmysql.dll 在 exe 同目录下\n"
                                      "3. 重新编译项目\n\n"
                                      "当前可用驱动: %1").arg(drivers.join(", ")));
        return;
    }

    // 检查是否已存在连接，如果存在则移除
    if (QSqlDatabase::contains("qt_sql_default_connection")) {
        QSqlDatabase::removeDatabase("qt_sql_default_connection");
    }

    // 从配置加载数据库连接参数
    loadDatabaseConfig();
    
    QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL");
    db.setHostName(m_dbHost);        // MySQL服务器地址（从配置读取）
    db.setPort(m_dbPort);            // MySQL端口（从配置读取）
    db.setDatabaseName(m_dbName);    // 数据库名称（从配置读取）
    db.setUserName(m_dbUsername);    // 用户名（从配置读取）
    db.setPassword(m_dbPassword);    // 密码（从配置读取）

    appendToLog(QString("尝试连接MySQL: %1@%2:%3/%4").arg(db.userName()).arg(db.hostName()).arg(db.port()).arg(db.databaseName()));

    if (!db.open()) {
        QString errorMsg = "MySQL数据库连接失败: " + db.lastError().text();
        appendToLog(errorMsg, true);
        qCritical() << errorMsg;
        QMessageBox::critical(nullptr, "数据库连接失败",
                              QString("无法连接到MySQL数据库！\n\n"
                                      "错误信息: %1\n\n"
                                      "请检查:\n"
                                      "1. MySQL服务是否运行\n"
                                      "2. 数据库名称、用户名、密码是否正确\n"
                                      "3. 防火墙是否允许连接").arg(db.lastError().text()));
        return;
    }

    qInfo() << "MySQL数据库连接成功";

    QSqlQuery query;
    // 数据记录表
    if (query.exec("CREATE TABLE IF NOT EXISTS data_records ("
                   "id INT PRIMARY KEY AUTO_INCREMENT,"
                   "slot_no INT,"
                   "status VARCHAR(255),"
                   "model_name VARCHAR(255),"
                   "model_code VARCHAR(255),"
                   "count INT,"
                   "time VARCHAR(255))")) {
        qDebug() << "数据记录表创建/检查成功";
    } else {
        qWarning() << "数据记录表创建失败:" << query.lastError().text();
    }

    // 车型绑定表
    if (query.exec("CREATE TABLE IF NOT EXISTS model_bindings ("
                   "id INT PRIMARY KEY AUTO_INCREMENT,"
                   "model_code VARCHAR(255),"
                   "model_name VARCHAR(255),"
                   "count INT)")) {
        qDebug() << "车型绑定表创建/检查成功";
    } else {
        qWarning() << "车型绑定表创建失败:" << query.lastError().text();
    }

    qInfo() << "数据库初始化完成";
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

void tcpClient::deleteDataRecord(int slotNo, const QString &status, const QString &modelName, const QString &modelCode, int count, const QString &currentTime) {
    QSqlQuery query;
    query.prepare("DELETE FROM data_records WHERE slot_no = ? AND status = ? AND model_name = ? AND model_code = ? AND count = ? AND time = ?");
    query.addBindValue(slotNo);
    query.addBindValue(status);
    query.addBindValue(modelName);
    query.addBindValue(modelCode);
    query.addBindValue(count);
    query.addBindValue(currentTime);
    if (!query.exec()) {
        appendToLog("删除数据记录失败: " + query.lastError().text(), true);
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

void tcpClient::updateModelBinding(const QString &oldModelCode, const QString &oldModelName, int oldCount, int row) {
    // 安全检查：确保表格项存在
    if (!ui || !ui->tableWidgetVehicleBinding) {
        qDebug() << "UI not ready for updateModelBinding";
        return;
    }

    QTableWidgetItem* item1 = ui->tableWidgetVehicleBinding->item(row, 1);
    QTableWidgetItem* item2 = ui->tableWidgetVehicleBinding->item(row, 2);
    QTableWidgetItem* item3 = ui->tableWidgetVehicleBinding->item(row, 3);

    if (!item1 || !item2 || !item3) {
        qDebug() << "Table items not found for row:" << row;
        return;
    }

    // 获取表格中的新值
    QString newModelCode = item1->text();
    QString newModelName = item2->text();
    QString newCountStr = item3->text();
    int newCount = newCountStr.toInt();

    QSqlQuery query;
    query.prepare("UPDATE model_bindings SET model_code = ?, model_name = ?, count = ? WHERE model_code = ? AND model_name = ? AND count = ?");
    query.addBindValue(newModelCode);
    query.addBindValue(newModelName);
    query.addBindValue(newCount);
    query.addBindValue(oldModelCode);
    query.addBindValue(oldModelName);
    query.addBindValue(oldCount);

    if (!query.exec()) {
        appendToLog("更新车型绑定失败: " + query.lastError().text(), true);
    } else {
        appendToLog(QString("车型绑定更新成功: %1 -> %2").arg(oldModelCode).arg(newModelCode));
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

// 密码管理函数实现

/**
 * @brief 初始化密码表
 */
void tcpClient::initPasswordTable()
{
    QSqlQuery query;
    query.exec("CREATE TABLE IF NOT EXISTS system_password ("
               "id INT PRIMARY KEY AUTO_INCREMENT,"
               "password VARCHAR(255),"
               "created_time VARCHAR(255))");
}

/**
 * @brief 保存密码到数据库
 * @param password 要保存的密码
 */
void tcpClient::savePassword(const QString &password)
{
    QSqlQuery query;
    // 先清空旧密码
    query.exec("DELETE FROM system_password");

    // 插入新密码
    query.prepare("INSERT INTO system_password (password, created_time) VALUES (?, ?)");
    query.addBindValue(password);
    query.addBindValue(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    if (!query.exec()) {
        appendToLog("保存密码失败: " + query.lastError().text(), true);
    } else {
        m_password = password;
        m_isPasswordSet = true;
        updatePasswordDisplay();
        appendToLog("密码设置成功");
    }
}

/**
 * @brief 从数据库加载密码
 * @return 存储的密码
 */
QString tcpClient::loadPassword()
{
    QSqlQuery query("SELECT password FROM system_password LIMIT 1");
    if (query.next()) {
        m_password = query.value(0).toString();
        m_isPasswordSet = !m_password.isEmpty();
        if (m_isPasswordSet) {
            appendToLog("密码已加载");
        }
    } else {
        m_password = "";
        m_isPasswordSet = false;
        appendToLog("未设置密码");
    }
    updatePasswordDisplay();
    return m_password;
}

/**
 * @brief 验证密码
 * @param inputPassword 输入的密码
 * @return 验证是否成功
 */
bool tcpClient::verifyPassword(const QString &inputPassword)
{
    if (!m_isPasswordSet) {
        return true; // 如果未设置密码，则允许访问
    }
    return inputPassword == m_password;
}

/**
 * @brief 显示密码输入对话框
 * @param title 对话框标题
 * @param message 提示消息
 * @return 密码验证是否成功
 */
bool tcpClient::showPasswordDialog(const QString &title, const QString &message)
{
    if (!m_isPasswordSet) {
        return true; // 如果未设置密码，则允许访问
    }

    bool ok;
    QString password = QInputDialog::getText(this, title, message,
                                             QLineEdit::Password, "", &ok);
    if (ok && !password.isEmpty()) {
        if (verifyPassword(password)) {
            return true;
        } else {
            QMessageBox::warning(this, "密码错误", "密码不正确，请重试！");
        }
    }
    return false;
}

/**
 * @brief 更新密码显示
 */
void tcpClient::updatePasswordDisplay()
{
    if (m_isPasswordSet && !m_password.isEmpty()) {
        // 显示密码的前3位和后1位，中间用*代替
        QString displayPassword = m_password.left(3) + QString("*").repeated(m_password.length() - 4) + m_password.right(1);
        ui->labelPassword->setText(QString("密码: %1").arg(displayPassword));
        // 浣跨敤Qt榛樿鏍峰紡`r`n        ui->labelPassword->setStyleSheet("");
    } else {
        ui->labelPassword->setText("密码: 未设置");
        // 浣跨敤Qt榛樿鏍峰紡`r`n        ui->labelPassword->setStyleSheet("");
    }
}

/**
 * @brief 设置密码按钮点击处理
 */
void tcpClient::onSetPasswordClicked()
{
    bool ok;
    QString newPassword = QInputDialog::getText(this, "设置密码",
                                                "请输入新的系统密码:",
                                                QLineEdit::Password, "", &ok);
    if (ok && !newPassword.isEmpty()) {
        // 如果已经设置了密码，需要先验证旧密码
        if (m_isPasswordSet) {
            QString oldPassword = QInputDialog::getText(this, "验证旧密码",
                                                        "请输入当前密码:",
                                                        QLineEdit::Password, "", &ok);
            if (!ok || !verifyPassword(oldPassword)) {
                QMessageBox::warning(this, "密码错误", "当前密码不正确！");
                return;
            }
        }

        // 确认新密码
        QString confirmPassword = QInputDialog::getText(this, "确认密码",
                                                        "请再次输入新密码:",
                                                        QLineEdit::Password, "", &ok);
        if (ok && newPassword == confirmPassword) {
            savePassword(newPassword);
            QMessageBox::information(this, "成功", "密码设置成功！");
        } else {
            QMessageBox::warning(this, "错误", "两次输入的密码不一致！");
        }
    }
}

/**
 * @brief 重写关闭事件，添加密码验证
 */
void tcpClient::closeEvent(QCloseEvent *event)
{
    if (m_isPasswordSet) {
        if (!showPasswordDialog("退出验证", "请输入密码以退出程序:")) {
            event->ignore();
            return;
        }
    }
    event->accept();
}

// 静态成员变量定义
tcpClient* tcpClient::s_instance = nullptr;

// 日志系统实现

/**
 * @brief 初始化日志系统
 */
void tcpClient::initLogSystem()
{
    // 设置日志目录
    setupLogDirectory();

    // 设置日志文件
    setupLogFile();

    // 清理旧日志文件
    cleanupLogFiles();

    // 安装自定义日志消息处理器
    qInstallMessageHandler(logMessageHandler);

    qDebug() << "日志系统初始化完成，日志目录:" << m_logDirectory;
}

/**
 * @brief 设置日志目录
 */
void tcpClient::setupLogDirectory()
{
    // 获取应用程序目录
    QString appDir = QCoreApplication::applicationDirPath();
    m_logDirectory = appDir + "/log";

    // 创建日志目录（如果不存在）
    QDir dir;
    if (!dir.exists(m_logDirectory)) {
        if (dir.mkpath(m_logDirectory)) {
            qDebug() << "创建日志目录成功:" << m_logDirectory;
        } else {
            qWarning() << "创建日志目录失败:" << m_logDirectory;
            // 如果创建失败，使用临时目录
            m_logDirectory = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/agt_logs";
            if (!dir.exists(m_logDirectory)) {
                dir.mkpath(m_logDirectory);
            }
        }
    }
}

/**
 * @brief 设置日志文件
 */
void tcpClient::setupLogFile()
{
    // 生成日志文件名（包含日期）
    QString currentDate = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    m_logFileName = QString("agt_client_%1.log").arg(currentDate);
    QString fullLogPath = m_logDirectory + "/" + m_logFileName;

    // 创建日志文件对象
    m_logFile = new QFile(fullLogPath);
    if (m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_logStream = new QTextStream(m_logFile);
        // Qt 6中默认使用UTF-8编码，无需设置

        // 写入日志文件头
        QString header = QString("\n=== AGT滑槽记录表客户端日志 - %1 ===\n")
                             .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
        *m_logStream << header << Qt::endl;
        m_logStream->flush();

        qDebug() << "日志文件创建成功:" << fullLogPath;
    } else {
        qWarning() << "无法创建日志文件:" << fullLogPath;
        delete m_logFile;
        m_logFile = nullptr;
    }
}

/**
 * @brief 清理旧日志文件
 */
void tcpClient::cleanupLogFiles()
{
    QDir logDir(m_logDirectory);
    if (!logDir.exists()) {
        return;
    }

    // 获取所有日志文件
    QStringList filters;
    filters << "agt_client_*.log";
    QFileInfoList fileList = logDir.entryInfoList(filters, QDir::Files, QDir::Time);

    // 删除7天前的日志文件
    QDateTime cutoffDate = QDateTime::currentDateTime().addDays(-7);
    int deletedCount = 0;

    for (const QFileInfo& fileInfo : fileList) {
        if (fileInfo.lastModified() < cutoffDate) {
            if (QFile::remove(fileInfo.absoluteFilePath())) {
                deletedCount++;
                qDebug() << "删除旧日志文件:" << fileInfo.fileName();
            }
        }
    }

    if (deletedCount > 0) {
        qDebug() << QString("清理完成，删除了 %1 个旧日志文件").arg(deletedCount);
    }
}

/**
 * @brief 日志消息处理器（静态函数）
 */
void tcpClient::logMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // 如果实例不存在，使用默认处理器
    if (!s_instance || !s_instance->m_logStream) {
        return;
    }

    QMutexLocker locker(&s_instance->m_logMutex);

    // 格式化日志消息
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString levelStr;
    QString colorCode;

    switch (type) {
    case QtDebugMsg:
        levelStr = "DEBUG";
        colorCode = "";
        break;
    case QtInfoMsg:
        levelStr = "INFO";
        colorCode = "";
        break;
    case QtWarningMsg:
        levelStr = "WARN";
        colorCode = "";
        break;
    case QtCriticalMsg:
        levelStr = "ERROR";
        colorCode = "";
        break;
    case QtFatalMsg:
        levelStr = "FATAL";
        colorCode = "";
        break;
    }

    // 构建完整的日志消息
    QString logMessage = QString("[%1] [%2] %3")
                             .arg(timestamp)
                             .arg(levelStr)
                             .arg(msg);

    // 如果有文件和行号信息，添加到日志中
    if (context.file && context.line > 0) {
        QString fileName = QFileInfo(context.file).fileName();
        logMessage += QString(" (%1:%2)").arg(fileName).arg(context.line);
    }

    // 写入日志文件
    *s_instance->m_logStream << logMessage << Qt::endl;
    s_instance->m_logStream->flush();

// 同时输出到控制台（用于调试）
#ifdef QT_DEBUG
    QTextStream consoleStream(stdout);
    consoleStream << logMessage << Qt::endl;
    #endif
}

// 数据库配置管理函数实现

/**
 * @brief 从配置文件加载数据库配置
 */
void tcpClient::loadDatabaseConfig()
{
    QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
    
    // 从配置文件读取，如果没有则使用默认值
    m_dbHost = settings.value("Database/Host", "localhost").toString();
    m_dbPort = settings.value("Database/Port", 3306).toInt();
    m_dbName = settings.value("Database/Name", "agt_database").toString();
    m_dbUsername = settings.value("Database/Username", "agt_user1").toString();
    m_dbPassword = settings.value("Database/Password", "root123").toString();
    
    // 更新UI界面显示
    if (ui) {
        ui->lineEditDbHost->setText(m_dbHost);
        ui->lineEditDbPort->setText(QString::number(m_dbPort));
        ui->lineEditDbName->setText(m_dbName);
        ui->lineEditDbUsername->setText(m_dbUsername);
        ui->lineEditDbPassword->setText(m_dbPassword);
    }
    
    qDebug() << "数据库配置已加载:" << m_dbHost << m_dbPort << m_dbName << m_dbUsername;
}

/**
 * @brief 保存数据库配置到配置文件
 */
void tcpClient::saveDatabaseConfig()
{
    // 从UI获取配置
    QString host = ui->lineEditDbHost->text().trimmed();
    QString portStr = ui->lineEditDbPort->text().trimmed();
    QString database = ui->lineEditDbName->text().trimmed();
    QString username = ui->lineEditDbUsername->text().trimmed();
    QString password = ui->lineEditDbPassword->text();
    
    // 验证输入
    if (host.isEmpty()) {
        QMessageBox::warning(this, "配置错误", "主机地址不能为空！");
        return;
    }
    
    bool ok;
    int port = portStr.toInt(&ok);
    if (!ok || port <= 0 || port > 65535) {
        QMessageBox::warning(this, "配置错误", "端口号必须是1-65535之间的数字！");
        return;
    }
    
    if (database.isEmpty()) {
        QMessageBox::warning(this, "配置错误", "数据库名称不能为空！");
        return;
    }
    
    if (username.isEmpty()) {
        QMessageBox::warning(this, "配置错误", "用户名不能为空！");
        return;
    }
    
    // 保存到配置文件
    QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
    settings.setValue("Database/Host", host);
    settings.setValue("Database/Port", port);
    settings.setValue("Database/Name", database);
    settings.setValue("Database/Username", username);
    settings.setValue("Database/Password", password);
    settings.sync();
    
    // 更新成员变量
    m_dbHost = host;
    m_dbPort = port;
    m_dbName = database;
    m_dbUsername = username;
    m_dbPassword = password;
    
    appendToLog(QString("数据库配置已保存: %1@%2:%3/%4").arg(username).arg(host).arg(port).arg(database));
    QMessageBox::information(this, "保存成功", 
                            QString("数据库配置已保存！\n\n"
                                   "配置将在下次启动程序时生效。\n"
                                   "如需立即生效，请重启程序。"));
}

/**
 * @brief 测试数据库连接
 */
bool tcpClient::testDatabaseConnection(const QString &host, int port, const QString &database, 
                                       const QString &username, const QString &password)
{
    // 检查可用的数据库驱动
    QStringList drivers = QSqlDatabase::drivers();
    if (!drivers.contains("QMYSQL")) {
        QMessageBox::critical(this, "驱动错误", "QMYSQL驱动不可用！");
        return false;
    }
    
    // 创建临时连接进行测试
    QString testConnectionName = "test_connection_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    QSqlDatabase testDb = QSqlDatabase::addDatabase("QMYSQL", testConnectionName);
    testDb.setHostName(host);
    testDb.setPort(port);
    testDb.setDatabaseName(database);
    testDb.setUserName(username);
    testDb.setPassword(password);
    
    bool success = testDb.open();
    QString errorMsg = testDb.lastError().text();
    
    // 关闭测试连接
    testDb.close();
    QSqlDatabase::removeDatabase(testConnectionName);
    
    return success;
}

/**
 * @brief 保存数据库配置按钮点击处理
 */
void tcpClient::onSaveDatabaseConfigClicked()
{
    saveDatabaseConfig();
}

/**
 * @brief 测试数据库连接按钮点击处理
 */
void tcpClient::onTestDatabaseConnectionClicked()
{
    // 从UI获取配置
    QString host = ui->lineEditDbHost->text().trimmed();
    QString portStr = ui->lineEditDbPort->text().trimmed();
    QString database = ui->lineEditDbName->text().trimmed();
    QString username = ui->lineEditDbUsername->text().trimmed();
    QString password = ui->lineEditDbPassword->text();
    
    // 验证输入
    if (host.isEmpty()) {
        QMessageBox::warning(this, "配置错误", "主机地址不能为空！");
        return;
    }
    
    bool ok;
    int port = portStr.toInt(&ok);
    if (!ok || port <= 0 || port > 65535) {
        QMessageBox::warning(this, "配置错误", "端口号必须是1-65535之间的数字！");
        return;
    }
    
    if (database.isEmpty()) {
        QMessageBox::warning(this, "配置错误", "数据库名称不能为空！");
        return;
    }
    
    if (username.isEmpty()) {
        QMessageBox::warning(this, "配置错误", "用户名不能为空！");
        return;
    }
    
    // 显示测试中提示
    appendToLog(QString("正在测试数据库连接: %1@%2:%3/%4...").arg(username).arg(host).arg(port).arg(database));
    ui->pushButtonTestDbConnection->setEnabled(false);
    ui->pushButtonTestDbConnection->setText("测试中...");
    
    // 测试连接
    bool success = testDatabaseConnection(host, port, database, username, password);
    
    // 恢复按钮状态
    ui->pushButtonTestDbConnection->setEnabled(true);
    ui->pushButtonTestDbConnection->setText("测试连接");
    
    if (success) {
        appendToLog("数据库连接测试成功！", false);
        QMessageBox::information(this, "连接成功", 
                                QString("数据库连接测试成功！\n\n"
                                       "主机: %1\n"
                                       "端口: %2\n"
                                       "数据库: %3\n"
                                       "用户名: %4").arg(host).arg(port).arg(database).arg(username));
    } else {
        appendToLog("数据库连接测试失败！", true);
        QMessageBox::critical(this, "连接失败", 
                             QString("数据库连接测试失败！\n\n"
                                    "请检查:\n"
                                    "1. MySQL服务是否运行\n"
                                    "2. 主机地址、端口是否正确\n"
                                    "3. 数据库名称、用户名、密码是否正确\n"
                                    "4. 防火墙是否允许连接\n"
                                    "5. 用户是否有访问权限"));
    }
}
