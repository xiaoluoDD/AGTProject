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
#include <QMouseEvent>
#include <QEvent>
#include <QLoggingCategory>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QSettings>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QGroupBox>
#include <QLineEdit>
#include <QLabel>
#include <QSpacerItem>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QColor>
#include <QBrush>
#include <QVector>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QTimeEdit>
#include <QTime>
#include <QPainter>

/**
 * @brief TwoLevelHeaderView构造函数
 */
TwoLevelHeaderView::TwoLevelHeaderView(Qt::Orientation orientation, QWidget *parent)
    : QHeaderView(orientation, parent)
{
    setDefaultSectionSize(80);
    setMinimumSectionSize(50);
    setMinimumHeight(80);
    setFixedHeight(80); // 固定高度确保二级表头有足够空间
}

/**
 * @brief 设置一级表头文本
 */
void TwoLevelHeaderView::setFirstLevelHeader(int section, const QString &text)
{
    m_firstLevelHeaders[section] = text;
}

/**
 * @brief 设置二级表头文本
 */
void TwoLevelHeaderView::setSecondLevelHeader(int section, const QString &text)
{
    m_secondLevelHeaders[section] = text;
}

/**
 * @brief 设置列索引到一级表头索引的映射
 */
void TwoLevelHeaderView::setSectionToFirstLevel(int section, int firstLevelIndex)
{
    m_sectionToFirstLevel[section] = firstLevelIndex;
}

/**
 * @brief 重写paintEvent以确保二级表头正确绘制
 */
void TwoLevelHeaderView::paintEvent(QPaintEvent *e)
{
    // 先调用基类方法绘制基础表头
    QHeaderView::paintEvent(e);
    
    // 绘制一级表头（跨越多个列）
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 遍历所有一级表头组
    QSet<int> processedFirstLevels; // 记录已处理的一级表头
    
    for (int col = 0; col < count(); ++col) {
        int firstLevelIndex = m_sectionToFirstLevel.value(col, -1);
        if (firstLevelIndex >= 0 && m_firstLevelHeaders.contains(firstLevelIndex)) {
            if (processedFirstLevels.contains(firstLevelIndex)) {
                continue; // 已经处理过这个一级表头
            }
            processedFirstLevels.insert(firstLevelIndex);
            
            // 找到该一级表头的所有列
            QList<int> relatedColumns;
            for (int i = 0; i < count(); ++i) {
                if (m_sectionToFirstLevel.value(i, -1) == firstLevelIndex) {
                    relatedColumns.append(i);
                }
            }
            
            if (relatedColumns.isEmpty()) {
                continue;
            }
            
            // 计算一级表头的区域
            int firstCol = relatedColumns.first();
            int startX = sectionPosition(firstCol) - offset();
            int totalWidth = 0;
            for (int c : relatedColumns) {
                totalWidth += sectionSize(c);
            }
            
            int headerHeight = viewport()->height();
            QRect firstLevelRect(startX, 0, totalWidth, headerHeight / 2);
            
            // 只绘制可见区域
            if (firstLevelRect.right() < 0 || firstLevelRect.left() > viewport()->width()) {
                continue; // 不在可见区域，跳过
            }
            
            // 绘制一级表头背景
            QStyleOptionHeader option;
            initStyleOption(&option);
            option.rect = firstLevelRect;
            option.section = firstCol;
            option.textAlignment = Qt::AlignCenter;
            style()->drawControl(QStyle::CE_HeaderSection, &option, &painter, this);
            
            // 绘制一级表头文本
            QString firstLevelText = m_firstLevelHeaders[firstLevelIndex];
            painter.setPen(palette().color(QPalette::Text));
            painter.drawText(firstLevelRect, Qt::AlignCenter, firstLevelText);
        }
    }
}

/**
 * @brief 绘制表头区域
 */
void TwoLevelHeaderView::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const
{
    if (!rect.isValid() || !painter) {
        return;
    }
    
    painter->save();
    
    // 获取一级表头索引
    int firstLevelIndex = m_sectionToFirstLevel.value(logicalIndex, -1);
    
    // 判断是否是二级表头列
    bool hasSecondLevel = m_secondLevelHeaders.contains(logicalIndex);
    bool hasFirstLevel = (firstLevelIndex >= 0 && m_firstLevelHeaders.contains(firstLevelIndex));
    
    if (hasFirstLevel) {
        // 只绘制二级表头部分（下半部分）
        // 一级表头在paintEvent中统一绘制
        // 如果有二级表头文本，才绘制二级表头部分
        if (hasSecondLevel) {
            QRect secondLevelRect = rect;
            secondLevelRect.setTop(rect.top() + rect.height() / 2);
            secondLevelRect.setHeight(rect.height() / 2);
            
            // 绘制二级表头背景
            QStyleOptionHeader secondOption;
            initStyleOption(&secondOption);
            secondOption.rect = secondLevelRect;
            secondOption.section = logicalIndex;
            secondOption.textAlignment = Qt::AlignCenter;
            style()->drawControl(QStyle::CE_HeaderSection, &secondOption, painter, this);
            
            // 绘制二级表头文本
            QString secondLevelText = m_secondLevelHeaders.value(logicalIndex, "");
            painter->setPen(palette().color(QPalette::Text));
            painter->drawText(secondLevelRect, Qt::AlignCenter, secondLevelText);
        }
        // 如果没有二级表头文本，则不绘制二级表头部分（休息列的情况）
    } else if (hasSecondLevel) {
        // 只有二级表头，没有一级表头（单级表头）
        QStyleOptionHeader option;
        initStyleOption(&option);
        option.rect = rect;
        option.section = logicalIndex;
        option.textAlignment = Qt::AlignCenter;
        style()->drawControl(QStyle::CE_HeaderSection, &option, painter, this);
        QString text = m_secondLevelHeaders.value(logicalIndex, "");
        painter->setPen(palette().color(QPalette::Text));
        painter->drawText(rect, Qt::AlignCenter, text);
    } else {
        // 普通表头，调用基类方法
        QHeaderView::paintSection(painter, rect, logicalIndex);
    }
    
    painter->restore();
}

/**
 * @brief 计算表头区域大小
 */
QSize TwoLevelHeaderView::sectionSizeFromContents(int logicalIndex) const
{
    QSize size = QHeaderView::sectionSizeFromContents(logicalIndex);
    // 增加高度以容纳两级表头
    if (m_secondLevelHeaders.contains(logicalIndex) || m_sectionToFirstLevel.contains(logicalIndex)) {
        size.setHeight(qMax(size.height() * 2, 80));
    }
    return size;
}

/**
 * @brief 重写sizeHint以确保表头有足够的高度
 */
QSize TwoLevelHeaderView::sizeHint() const
{
    QSize size = QHeaderView::sizeHint();
    // 如果有二级表头，增加高度
    if (!m_secondLevelHeaders.isEmpty() || !m_sectionToFirstLevel.isEmpty()) {
        size.setHeight(qMax(size.height(), 80));
    }
    return size;
}

/**
 * @brief 重写minimumSizeHint以确保表头有最小高度
 */
QSize TwoLevelHeaderView::minimumSizeHint() const
{
    QSize size = QHeaderView::minimumSizeHint();
    if (!m_secondLevelHeaders.isEmpty() || !m_sectionToFirstLevel.isEmpty()) {
        size.setHeight(qMax(size.height(), 80));
    }
    return size;
}

/**
 * @brief 构造函数
 * @param parent 父窗口指针
 */
tcpClient::tcpClient(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::tcpClient)
    , m_socket(new QTcpSocket(this))
    , m_serverSocket(new QTcpSocket(this))
    , m_edSoftwareSocket(new QTcpSocket(this))
    , m_connectionTimer(new QTimer(this))
    , m_serverConnectionTimer(new QTimer(this))
    , m_edSoftwareConnectionTimer(new QTimer(this))
    , m_shiftCheckTimer(new QTimer(this))
    , m_dayShiftStart(7, 15, 0)      // 默认白班开始时间
    , m_dayShiftEnd(17, 30, 0)       // 默认白班结束时间
    , m_nightShiftStart(17, 30, 0)    // 默认夜班开始时间
    , m_nightShiftEnd(7, 15, 0)       // 默认夜班结束时间
    , m_shiftConfigLoaded(false)      // 班次设置未加载
    , m_visualizationDataTimer(new QTimer(this))
    , m_projectGroupDataTimer(new QTimer(this))
    , m_exceptionDataTimer(new QTimer(this))
    , m_shiftDisplayAutoResetTimer(new QTimer(this))
    , m_plcAutoReconnectTimer(new QTimer(this))
    , m_currentShiftTableDailyClearTimer(new QTimer(this))
    , m_projectGroupShiftAutoResetTimer(new QTimer(this))
    , m_isConnected(false)
    , m_isServerConnected(false)
    , m_isEdSoftwareConnected(false)
    , m_isAutoReconnecting(false)
    , m_fullTrayCount(0)
    , m_password("")
    , m_isPasswordSet(false)
    , m_lastCurrentShiftTableClearDate(QDate())
    , plannedCountLabel(nullptr)
    , actualCountLabel(nullptr)
    , delayedCountLabel(nullptr)
    , exceptionTableWidget(nullptr)
    , pushButtonCurrentShiftTablePage(nullptr)
    , currentShiftTablePage(nullptr)
    , currentShiftTableWidget(nullptr)
    , realTrayInTableWidget(nullptr)
    , realTrayOutTableWidget(nullptr)
    , emptyTrayInTableWidget(nullptr)
    , emptyTrayOutTableWidget(nullptr)
    , m_plannedCount(100)
    , m_actualCount(89)
    , m_delayedCount(0)
    , m_currentDisplayShift("current")
    , m_projectGroupDisplayShift("current")
    , m_displayedPlannedCount(100)
    , m_displayedActualCount(89)
    , m_displayedDelayedCount(0)
    , m_realTrayBatchCount(0)
    , m_emptyTrayBatchCount(0)
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
        
        loadCurrentShiftRecordsFromDb();
        qDebug() << "loadCurrentShiftRecordsFromDb completed";

        // 加载可视化记录（在数据库初始化完成后，且可视化页面已创建）
        // 使用延迟加载，确保UI完全初始化
        QTimer::singleShot(200, this, [this]() {
            loadVisualizationRecords();
            qDebug() << "loadVisualizationRecords completed";
            
            // 加载空托盘可视化记录
            loadEmptyTrayVisualizationRecords();
            qDebug() << "loadEmptyTrayVisualizationRecords completed";
            
            // 加载统计信息
            loadStatisticsInfo();
            qDebug() << "loadStatisticsInfo completed";
            
            // 加载异常记录
            loadExceptionRecords();
            qDebug() << "loadExceptionRecords completed";
        });

        // 加载连接配置
        loadConnectionConfig();
        qDebug() << "loadConnectionConfig completed";
        
        // 加载班次设置（延迟加载，确保UI已准备好）
        QTimer::singleShot(300, this, [this]() {
            loadShiftConfig();
            qDebug() << "loadShiftConfig completed";
        });

        // 连接PLC Socket信号槽
        connect(m_socket, &QTcpSocket::connected, this, &tcpClient::onSocketConnected);
        connect(m_socket, &QTcpSocket::disconnected, this, &tcpClient::onSocketDisconnected);
        connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
                this, &tcpClient::onSocketError);
        connect(m_socket, &QTcpSocket::readyRead, this, &tcpClient::onSocketReadyRead);

        // 连接PLC超时定时器
        connect(m_connectionTimer, &QTimer::timeout, this, &tcpClient::onConnectionTimeout);

        // 设置PLC连接超时时间（5秒）
        m_connectionTimer->setSingleShot(true);
        m_connectionTimer->setInterval(5000);
        
        // 连接PLC自动重连定时器
        connect(m_plcAutoReconnectTimer, &QTimer::timeout, this, &tcpClient::onPlcAutoReconnect);
        
        // 设置PLC自动重连定时器（每3秒检测一次）
        m_plcAutoReconnectTimer->setSingleShot(false);
        m_plcAutoReconnectTimer->setInterval(3000);
        
        // 连接服务端Socket信号槽
        connect(m_serverSocket, &QTcpSocket::connected, this, &tcpClient::onServerSocketConnected);
        connect(m_serverSocket, &QTcpSocket::disconnected, this, &tcpClient::onServerSocketDisconnected);
        connect(m_serverSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
                this, &tcpClient::onServerSocketError);
        connect(m_serverSocket, &QTcpSocket::readyRead, this, &tcpClient::onServerSocketReadyRead);

        // 连接服务端超时定时器
        connect(m_serverConnectionTimer, &QTimer::timeout, this, &tcpClient::onServerConnectionTimeout);

        // 设置服务端连接超时时间（5秒）
        m_serverConnectionTimer->setSingleShot(true);
        m_serverConnectionTimer->setInterval(5000);
        
        // 连接ED软件Socket信号槽
        connect(m_edSoftwareSocket, &QTcpSocket::connected, this, &tcpClient::onEdSoftwareSocketConnected);
        connect(m_edSoftwareSocket, &QTcpSocket::disconnected, this, &tcpClient::onEdSoftwareSocketDisconnected);
        connect(m_edSoftwareSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
                this, &tcpClient::onEdSoftwareSocketError);
        connect(m_edSoftwareSocket, &QTcpSocket::readyRead, this, &tcpClient::onEdSoftwareSocketReadyRead);

        // 连接ED软件超时定时器
        connect(m_edSoftwareConnectionTimer, &QTimer::timeout, this, &tcpClient::onEdSoftwareConnectionTimeout);

        // 设置ED软件连接超时时间（5秒）
        m_edSoftwareConnectionTimer->setSingleShot(true);
        m_edSoftwareConnectionTimer->setInterval(5000);
        
        // 初始化班次检查定时器（每分钟检查一次）
        connect(m_shiftCheckTimer, &QTimer::timeout, this, &tcpClient::checkShiftChange);
        m_shiftCheckTimer->setInterval(60000); // 60秒 = 1分钟
        m_shiftCheckTimer->start();
        
        // 初始化当前班次表格每日清空定时器（每分钟检查一次，凌晨6点清空）
        connect(m_currentShiftTableDailyClearTimer, &QTimer::timeout, this, &tcpClient::onCurrentShiftTableDailyClear);
        m_currentShiftTableDailyClearTimer->setInterval(60000); // 60秒 = 1分钟
        m_currentShiftTableDailyClearTimer->start();
        
        // 初始化工程组记录界面班次自动恢复定时器（1分钟后恢复）
        connect(m_projectGroupShiftAutoResetTimer, &QTimer::timeout, this, &tcpClient::onProjectGroupShiftAutoReset);
        m_projectGroupShiftAutoResetTimer->setSingleShot(true);
        m_projectGroupShiftAutoResetTimer->setInterval(60000); // 60秒 = 1分钟
        
        // 初始化可视化数据发送定时器（每3秒触发一次，立即发送AGT搬运数据，1秒后发送工程组数据，2秒后发送异常记录）
        connect(m_visualizationDataTimer, &QTimer::timeout, this, &tcpClient::sendVisualizationDataToServer);
        m_visualizationDataTimer->setInterval(3000); // 3秒
        // 注意：定时器在服务端连接成功后才启动
        
        // 初始化工程组数据发送定时器（单次触发，1秒后发送工程组数据）
        connect(m_projectGroupDataTimer, &QTimer::timeout, this, &tcpClient::sendProjectGroupDataToServer);
        m_projectGroupDataTimer->setSingleShot(true);
        m_projectGroupDataTimer->setInterval(1000); // 1秒
        
        // 初始化异常数据发送定时器（单次触发，发送工程组后1秒发送异常记录）
        connect(m_exceptionDataTimer, &QTimer::timeout, this, &tcpClient::sendExceptionDataToServer);
        m_exceptionDataTimer->setSingleShot(true);
        m_exceptionDataTimer->setInterval(1000); // 1秒
        
        // 初始化可视化数组（21个槽位，3列7行，位置0-20）
        m_realTraySlots.resize(21);
        for (int i = 0; i < 21; ++i) {
            m_realTraySlots[i] = "";
        }
        
        // 初始化空托盘可视化数组（21个槽位，3列7行，位置0-20）
        m_emptyTraySlots.resize(21);
        for (int i = 0; i < 21; ++i) {
            m_emptyTraySlots[i] = "";
        }

        updateConnectionStatus(false);

        // 在UI完全初始化后记录启动日志
        appendToLog("TCP客户端已启动");
        qInfo() << "AGT滑槽记录表客户端启动完成";
        
        // 如果已有保存的PLC连接配置且未连接，启动自动重连
        QString serverIP = ui->lineEditServerIP->text().trimmed();
        QString portStr = ui->lineEditPort->text().trimmed();
        if (!serverIP.isEmpty() && !portStr.isEmpty()) {
            bool ok;
            int port = portStr.toInt(&ok);
            if (ok && port > 0 && port <= 65535) {
                // 延迟启动自动重连，确保UI完全初始化
                QTimer::singleShot(1000, this, [this]() {
                    if (!m_isConnected && m_socket->state() != QAbstractSocket::ConnectedState) {
                        m_plcAutoReconnectTimer->start();
                        // 自动重连不写入日志
                    }
                });
            }
        }

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
    if (m_serverSocket && m_serverSocket->state() == QAbstractSocket::ConnectedState) {
        m_serverSocket->disconnectFromHost();
    }
    if (m_edSoftwareSocket && m_edSoftwareSocket->state() == QAbstractSocket::ConnectedState) {
        m_edSoftwareSocket->disconnectFromHost();
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

    // 创建当前班次表格页面和按钮
    pushButtonCurrentShiftTablePage = new QPushButton(this);
    pushButtonCurrentShiftTablePage->setText("当前班次表格");
    pushButtonCurrentShiftTablePage->setCheckable(true);
    pushButtonCurrentShiftTablePage->setObjectName("pushButtonCurrentShiftTablePage");
    
    // 将当前班次表格按钮插入到数据表格按钮之前（索引1）
    ui->horizontalLayout_4->insertWidget(1, pushButtonCurrentShiftTablePage);
    
    // 创建可视化记录页面和按钮
    pushButtonVisualizationPage = new QPushButton(this);
    pushButtonVisualizationPage->setText("AGT搬运");
    pushButtonVisualizationPage->setCheckable(true);
    pushButtonVisualizationPage->setObjectName("pushButtonVisualizationPage");
    
    // 将可视化记录按钮插入到数据表格和车型绑定按钮之间（索引3，因为插入了当前班次表格）
    ui->horizontalLayout_4->insertWidget(3, pushButtonVisualizationPage);
    
    // 创建工程组记录页面和按钮
    pushButtonProjectGroupPage = new QPushButton(this);
    pushButtonProjectGroupPage->setText("工程组记录");
    pushButtonProjectGroupPage->setCheckable(true);
    pushButtonProjectGroupPage->setObjectName("pushButtonProjectGroupPage");
    
    // 将工程组记录按钮插入到可视化记录按钮之后（索引4，因为插入了当前班次表格）
    ui->horizontalLayout_4->insertWidget(4, pushButtonProjectGroupPage);
    
    // 创建总成指示表页面和按钮
    pushButtonAssemblyIndicatorPage = new QPushButton(this);
    pushButtonAssemblyIndicatorPage->setText("总成指示表");
    pushButtonAssemblyIndicatorPage->setCheckable(true);
    pushButtonAssemblyIndicatorPage->setObjectName("pushButtonAssemblyIndicatorPage");
    
    // 将总成指示表按钮插入到工程组记录按钮之后（索引5）
    ui->horizontalLayout_4->insertWidget(5, pushButtonAssemblyIndicatorPage);
    
    // 创建可视化记录页面
    visualizationPage = new QWidget();
    visualizationPage->setObjectName("visualizationPage");
    QVBoxLayout* visualizationLayout = new QVBoxLayout(visualizationPage);
    visualizationLayout->setObjectName("visualizationLayout");
    visualizationLayout->setSpacing(15);
    visualizationLayout->setContentsMargins(20, 20, 20, 20);
    
    // 创建AGT路线部品GroupBox，用于嵌套实滑槽和空滑槽记录
    QGroupBox* agtRoutePartsGroupBox = new QGroupBox(visualizationPage);
    agtRoutePartsGroupBox->setTitle("AGT路线部品");
    agtRoutePartsGroupBox->setStyleSheet(
        "QGroupBox {"
        "font-size: 14pt;"
        "font-weight: bold;"
        "border: 2px solid #d0d0d0;"
        "border-radius: 8px;"
        "margin-top: 10px;"
        "padding-top: 15px;"
        "}"
        "QGroupBox::title {"
        "subcontrol-origin: margin;"
        "subcontrol-position: top center;"
        "padding: 0 8px 0 8px;"
        "}"
    );
    
    // 创建AGT路线部品内部的布局，用于居中显示实滑槽和空滑槽记录
    QHBoxLayout* agtRoutePartsLayout = new QHBoxLayout(agtRoutePartsGroupBox);
    agtRoutePartsLayout->setSpacing(20);
    agtRoutePartsLayout->setContentsMargins(20, 20, 20, 20);
    
    // 创建横向布局，用于并排显示实滑槽和空滑槽记录
    QHBoxLayout* horizontalTrayLayout = new QHBoxLayout();
    horizontalTrayLayout->setSpacing(20);
    horizontalTrayLayout->setContentsMargins(0, 0, 0, 0);
    
    // 创建实滑槽记录GroupBox（左侧）
    QGroupBox* realTrayGroupBox = new QGroupBox(agtRoutePartsGroupBox);
    realTrayGroupBox->setTitle("实滑槽记录");
    realTrayGroupBox->setStyleSheet(
        "QGroupBox {"
        "font-size: 14pt;"
        "font-weight: bold;"
        "border: 2px solid #d0d0d0;"
        "border-radius: 8px;"
        "margin-top: 10px;"
        "padding-top: 15px;"
        "}"
        "QGroupBox::title {"
        "subcontrol-origin: margin;"
        "left: 15px;"
        "padding: 0 8px 0 8px;"
        "}"
    );
    
    // 使用网格布局，3列11行：入口（1行）+ 列标签（1行）+ 21个槽位（7行）+ 底部标签（1行）+ 出口（1行）
    QGridLayout* realTrayLayout = new QGridLayout(realTrayGroupBox);
    realTrayLayout->setSpacing(8);
    realTrayLayout->setContentsMargins(15, 20, 15, 15);
    m_realTrayLabels.clear();
    
    // 第0行：入口（居中，跨3列）
    QLabel* entranceLabel = new QLabel(realTrayGroupBox);
    entranceLabel->setAlignment(Qt::AlignCenter);
    entranceLabel->setMinimumSize(200, 45);
    entranceLabel->setStyleSheet(
        "QLabel {"
        "border: 2px solid #d0d0d0;"
        "border-radius: 8px;"
        "font-size: 12pt;"
        "font-weight: bold;"
        "padding: 5px;"
        "}"
    );
    entranceLabel->setText("三车间滑槽出口");
    m_realTrayLabels.append(entranceLabel);
    realTrayLayout->addWidget(entranceLabel, 0, 0, 1, 3, Qt::AlignCenter);
    
    // 第1行：列标签（2101、2102、2103）
    QStringList columnLabels = {"2101", "2102", "2103"};
    for (int col = 0; col < 3; ++col) {
        QLabel* columnLabel = new QLabel(realTrayGroupBox);
        columnLabel->setAlignment(Qt::AlignCenter);
        columnLabel->setFixedHeight(9);
        columnLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        columnLabel->setStyleSheet(
            "QLabel {"
            "border: none;"
            "font-size: 9pt;"
            "font-weight: normal;"
            "padding: 0px;"
            "background-color: transparent;"
            "}"
        );
        columnLabel->setText(columnLabels[col]);
        realTrayLayout->addWidget(columnLabel, 1, col);
    }
    
    // 第2-8行：21个槽位（每行3个）
    for (int row = 2; row <= 8; ++row) {
        for (int col = 0; col < 3; ++col) {
            int index = (row - 2) * 3 + col + 1; // 索引从1开始（跳过入口0）
            QLabel* label = new QLabel(realTrayGroupBox);
            label->setAlignment(Qt::AlignCenter);
            label->setMinimumSize(100, 45);
            label->setStyleSheet(
                "QLabel {"
                "border: 2px solid #d0d0d0;"
                "border-radius: 8px;"
                "font-size: 12pt;"
                "font-weight: bold;"
                "padding: 5px;"
                "}"
            );
            label->setText("");  // 初始为空
            label->installEventFilter(this); // 安装事件过滤器以支持双击
            m_realTrayLabels.append(label);
            realTrayLayout->addWidget(label, row, col);
        }
    }
    
    // 第9行：底部列标签（1001\n1101、1002\n1102、1003\n1103）
    QStringList bottomColumnLabels = {"1001\n1101", "1002\n1102", "1003\n1103"};
    for (int col = 0; col < 3; ++col) {
        QLabel* bottomColumnLabel = new QLabel(realTrayGroupBox);
        bottomColumnLabel->setAlignment(Qt::AlignCenter);
        bottomColumnLabel->setFixedHeight(28);
        bottomColumnLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        bottomColumnLabel->setStyleSheet(
            "QLabel {"
            "border: none;"
            "font-size: 9pt;"
            "font-weight: normal;"
            "padding: 0px;"
            "background-color: transparent;"
            "line-height: 1.0;"
            "}"
        );
        bottomColumnLabel->setText(bottomColumnLabels[col]);
        realTrayLayout->addWidget(bottomColumnLabel, 9, col);
    }
    
    // 第10行：出口（居中，跨3列）
    QLabel* exitLabel = new QLabel(realTrayGroupBox);
    exitLabel->setAlignment(Qt::AlignCenter);
    exitLabel->setMinimumSize(100, 45);
    exitLabel->setStyleSheet(
        "QLabel {"
        "border: 2px solid #d0d0d0;"
        "border-radius: 8px;"
        "font-size: 12pt;"
        "font-weight: bold;"
        "padding: 5px;"
        "}"
    );
    exitLabel->setText("二车间滑槽入口");
    m_realTrayExitLabelText = "二车间滑槽入口"; // 保存初始文本
    m_realTrayLabels.append(exitLabel);
    realTrayLayout->addWidget(exitLabel, 10, 0, 1, 3, Qt::AlignCenter);
    horizontalTrayLayout->addWidget(realTrayGroupBox, 1); // 添加拉伸因子，填充宽度
    
    // 创建空滑槽记录GroupBox（右侧）
    QGroupBox* emptyTrayGroupBox = new QGroupBox(agtRoutePartsGroupBox);
    emptyTrayGroupBox->setTitle("空滑槽记录");
    emptyTrayGroupBox->setStyleSheet(
        "QGroupBox {"
        "font-size: 14pt;"
        "font-weight: bold;"
        "border: 2px solid #d0d0d0;"
        "border-radius: 8px;"
        "margin-top: 10px;"
        "padding-top: 15px;"
        "}"
        "QGroupBox::title {"
        "subcontrol-origin: margin;"
        "left: 15px;"
        "padding: 0 8px 0 8px;"
        "}"
    );
    
    // 使用网格布局，3列11行：入口（1行）+ 列标签（1行）+ 21个槽位（7行）+ 底部标签（1行）+ 出口（1行）
    QGridLayout* emptyTrayLayout = new QGridLayout(emptyTrayGroupBox);
    emptyTrayLayout->setSpacing(8);
    emptyTrayLayout->setContentsMargins(15, 20, 15, 15);
    m_emptyTrayLabels.clear();
    
    // 第0行：入口（居中，跨3列）
    QLabel* emptyEntranceLabel = new QLabel(emptyTrayGroupBox);
    emptyEntranceLabel->setAlignment(Qt::AlignCenter);
    emptyEntranceLabel->setMinimumSize(200, 45);
    emptyEntranceLabel->setStyleSheet(
        "QLabel {"
        "border: 2px solid #d0d0d0;"
        "border-radius: 8px;"
        "font-size: 12pt;"
        "font-weight: bold;"
        "padding: 5px;"
        "}"
    );
    emptyEntranceLabel->setText("三车间滑槽入口");
    m_emptyTrayLabels.append(emptyEntranceLabel);
    emptyTrayLayout->addWidget(emptyEntranceLabel, 0, 0, 1, 3, Qt::AlignCenter);
    
    // 第1行：列标签（2003、2002、2001）
    QStringList emptyColumnLabels = {"2003", "2002", "2001"};
    for (int col = 0; col < 3; ++col) {
        QLabel* emptyColumnLabel = new QLabel(emptyTrayGroupBox);
        emptyColumnLabel->setAlignment(Qt::AlignCenter);
        emptyColumnLabel->setFixedHeight(9);
        emptyColumnLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        emptyColumnLabel->setStyleSheet(
            "QLabel {"
            "border: none;"
            "font-size: 9pt;"
            "font-weight: normal;"
            "padding: 0px;"
            "background-color: transparent;"
            "}"
        );
        emptyColumnLabel->setText(emptyColumnLabels[col]);
        emptyTrayLayout->addWidget(emptyColumnLabel, 1, col);
    }
    
    // 第2-8行：21个槽位（每行3个）
    for (int row = 2; row <= 8; ++row) {
        for (int col = 0; col < 3; ++col) {
            int index = (row - 2) * 3 + col + 1; // 索引从1开始（跳过入口0）
            QLabel* label = new QLabel(emptyTrayGroupBox);
            label->setAlignment(Qt::AlignCenter);
            label->setMinimumSize(100, 45);
            label->setStyleSheet(
                "QLabel {"
                "border: 2px solid #d0d0d0;"
                "border-radius: 8px;"
                "font-size: 12pt;"
                "font-weight: bold;"
                "padding: 5px;"
                "}"
            );
            label->setText("");  // 初始为空
            label->installEventFilter(this); // 安装事件过滤器以支持双击
            m_emptyTrayLabels.append(label);
            emptyTrayLayout->addWidget(label, row, col);
        }
    }
    
    // 第9行：底部列标签（1001\n1101、1002\n1102、1003\n1103）
    QStringList emptyBottomColumnLabels = {"1001\n1101", "1002\n1102", "1003\n1103"};
    for (int col = 0; col < 3; ++col) {
        QLabel* emptyBottomColumnLabel = new QLabel(emptyTrayGroupBox);
        emptyBottomColumnLabel->setAlignment(Qt::AlignCenter);
        emptyBottomColumnLabel->setFixedHeight(28);
        emptyBottomColumnLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        emptyBottomColumnLabel->setStyleSheet(
            "QLabel {"
            "border: none;"
            "font-size: 9pt;"
            "font-weight: normal;"
            "padding: 0px;"
            "background-color: transparent;"
            "line-height: 1.0;"
            "}"
        );
        emptyBottomColumnLabel->setText(emptyBottomColumnLabels[col]);
        emptyTrayLayout->addWidget(emptyBottomColumnLabel, 9, col);
    }
    
    // 第10行：出口（居中，跨3列）
    QLabel* emptyExitLabel = new QLabel(emptyTrayGroupBox);
    emptyExitLabel->setAlignment(Qt::AlignCenter);
    emptyExitLabel->setMinimumSize(100, 45);
    emptyExitLabel->setStyleSheet(
        "QLabel {"
        "border: 2px solid #d0d0d0;"
        "border-radius: 8px;"
        "font-size: 12pt;"
        "font-weight: bold;"
        "padding: 5px;"
        "}"
    );
    emptyExitLabel->setText("二车间滑槽出口");
    m_emptyTrayExitLabelText = "二车间滑槽出口"; // 保存初始文本
    m_emptyTrayLabels.append(emptyExitLabel);
    emptyTrayLayout->addWidget(emptyExitLabel, 10, 0, 1, 3, Qt::AlignCenter);
    horizontalTrayLayout->addWidget(emptyTrayGroupBox, 1); // 添加拉伸因子，填充宽度
    
    // 将横向布局添加到AGT路线部品布局中，填充整个宽度
    agtRoutePartsLayout->addLayout(horizontalTrayLayout);
    
    // 将AGT路线部品GroupBox直接添加到主布局中，填充整个宽度和可用高度
    visualizationLayout->addWidget(agtRoutePartsGroupBox, 1);
    
    // 创建底部区域（异常表格和统计信息）的水平布局
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(15);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    
    // 创建异常表格区域（左侧）
    QGroupBox* exceptionGroupBox = new QGroupBox(visualizationPage);
    exceptionGroupBox->setTitle("异常记录");
    exceptionGroupBox->setStyleSheet(
        "QGroupBox {"
        "font-size: 14pt;"
        "font-weight: bold;"
        "border: 2px solid #d0d0d0;"
        "border-radius: 8px;"
        "margin-top: 10px;"
        "padding-top: 15px;"
        "}"
        "QGroupBox::title {"
        "subcontrol-origin: margin;"
        "left: 15px;"
        "padding: 0 8px 0 8px;"
        "}"
    );
    
    QVBoxLayout* exceptionLayout = new QVBoxLayout(exceptionGroupBox);
    exceptionLayout->setContentsMargins(10, 20, 10, 10);
    
    // 创建异常表格
    exceptionTableWidget = new QTableWidget(exceptionGroupBox);
    exceptionTableWidget->setColumnCount(5);
    QStringList exceptionHeaders;
    exceptionHeaders << "滑槽号" << "车型名称" << "送入送出状态" << "异常信息" << "日期";
    exceptionTableWidget->setHorizontalHeaderLabels(exceptionHeaders);
    exceptionTableWidget->setStyleSheet(
        "QTableWidget {"
        "font-size: 10pt;"
        "gridline-color: #d0d0d0;"
        "background-color: white;"
        "alternate-background-color: #f5f5f5;"
        "}"
        "QTableWidget::item {"
        "padding: 5px;"
        "}"
        "QHeaderView::section {"
        "background-color: #e0e0e0;"
        "padding: 5px;"
        "font-weight: bold;"
        "border: 1px solid #d0d0d0;"
        "}"
    );
    exceptionTableWidget->setAlternatingRowColors(true);
    exceptionTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    exceptionTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    exceptionTableWidget->horizontalHeader()->setStretchLastSection(true);
    exceptionTableWidget->setColumnWidth(0, 80);   // 滑槽号
    exceptionTableWidget->setColumnWidth(1, 120);  // 车型名称
    exceptionTableWidget->setColumnWidth(2, 120); // 送入送出状态
    exceptionTableWidget->setColumnWidth(3, 200); // 异常信息
    exceptionTableWidget->setColumnWidth(4, 150); // 日期
    exceptionLayout->addWidget(exceptionTableWidget);
    
    bottomLayout->addWidget(exceptionGroupBox, 1); // 异常表格占50%宽度
    
    // 创建统计区域（右侧，宽度减小）
    QGroupBox* statisticsGroupBox = new QGroupBox(visualizationPage);
    statisticsGroupBox->setTitle("统计信息");
    statisticsGroupBox->setStyleSheet(
        "QGroupBox {"
        "font-size: 14pt;"
        "font-weight: bold;"
        "border: 2px solid #d0d0d0;"
        "border-radius: 8px;"
        "margin-top: 10px;"
        "padding-top: 15px;"
        "}"
        "QGroupBox::title {"
        "subcontrol-origin: margin;"
        "left: 15px;"
        "padding: 0 8px 0 8px;"
        "}"
    );
    
    QVBoxLayout* statisticsLayout = new QVBoxLayout(statisticsGroupBox);
    statisticsLayout->setSpacing(15);
    statisticsLayout->setContentsMargins(15, 20, 15, 15);
    
    // 计划便次
    plannedCountLabel = new QLabel(statisticsGroupBox);
    m_plannedCount = 100;
    m_displayedPlannedCount = m_plannedCount;
    plannedCountLabel->setText(QString("计划便次：%1便").arg(m_plannedCount));
    plannedCountLabel->setStyleSheet(
        "QLabel {"
        "font-size: 13pt;"
        "font-weight: bold;"
        "padding: 8px;"
        "}"
    );
    plannedCountLabel->setAlignment(Qt::AlignCenter);
    plannedCountLabel->installEventFilter(this); // 安装事件过滤器以支持双击
    statisticsLayout->addWidget(plannedCountLabel);
    
    // 实际便次
    actualCountLabel = new QLabel(statisticsGroupBox);
    m_actualCount = 89;
    m_displayedActualCount = m_actualCount;
    actualCountLabel->setText(QString("实际便次：%1便").arg(m_actualCount));
    actualCountLabel->setStyleSheet(
        "QLabel {"
        "font-size: 13pt;"
        "font-weight: bold;"
        "padding: 8px;"
        "}"
    );
    actualCountLabel->setAlignment(Qt::AlignCenter);
    actualCountLabel->installEventFilter(this); // 安装事件过滤器以支持双击
    statisticsLayout->addWidget(actualCountLabel);
    
    // 延迟便次
    delayedCountLabel = new QLabel(statisticsGroupBox);
    m_delayedCount = 0;
    m_displayedDelayedCount = m_delayedCount;
    delayedCountLabel->setText(QString("延迟便次：%1便").arg(m_delayedCount));
    delayedCountLabel->setStyleSheet(
        "QLabel {"
        "font-size: 13pt;"
        "font-weight: bold;"
        "padding: 8px;"
        "}"
    );
    delayedCountLabel->setAlignment(Qt::AlignCenter);
    delayedCountLabel->installEventFilter(this); // 安装事件过滤器以支持双击
    statisticsLayout->addWidget(delayedCountLabel);
    
    // 添加班次显示按钮
    shiftDisplayButton = new QPushButton(statisticsGroupBox);
    m_currentDisplayShift = "current"; // 初始显示当前班次
    updateShiftDisplayButton();
    shiftDisplayButton->setStyleSheet(
        "QPushButton {"
        "font-size: 11pt;"
        "font-weight: bold;"
        "padding: 6px 12px;"
        "min-width: 80px;"
        "border: 2px solid #4CAF50;"
        "border-radius: 5px;"
        "background-color: #E8F5E9;"
        "}"
        "QPushButton:hover {"
        "background-color: #C8E6C9;"
        "}"
    );
    connect(shiftDisplayButton, &QPushButton::clicked, this, &tcpClient::onShiftDisplayButtonClicked);
    statisticsLayout->addWidget(shiftDisplayButton);
    
    bottomLayout->addWidget(statisticsGroupBox, 1); // 统计信息占50%宽度
    
    // 将底部区域添加到主布局中
    visualizationLayout->addLayout(bottomLayout);
    
    // 注意：可视化记录的加载将在数据库初始化完成后进行
    // 不在这里调用 loadVisualizationRecords()，避免数据库未初始化的问题
    
    // 创建当前班次表格页面（按状态分为4个表格）
    currentShiftTablePage = new QWidget();
    currentShiftTablePage->setObjectName("currentShiftTablePage");
    QVBoxLayout* currentShiftTableLayout = new QVBoxLayout(currentShiftTablePage);
    currentShiftTableLayout->setObjectName("currentShiftTableLayout");
    currentShiftTableLayout->setSpacing(10);
    currentShiftTableLayout->setContentsMargins(15, 15, 15, 15);
    
    // 创建2x2网格布局用于放置4个表格
    QGridLayout* gridLayout = new QGridLayout();
    gridLayout->setSpacing(10);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    
    // 创建表格的辅助函数
    auto createTableGroupBox = [](QWidget* parent, const QString& title, QTableWidget** tableWidget) -> QGroupBox* {
        QGroupBox* groupBox = new QGroupBox(parent);
        groupBox->setTitle(title);
        QVBoxLayout* layout = new QVBoxLayout(groupBox);
        layout->setSpacing(5);
        layout->setContentsMargins(10, 15, 10, 10);
        
        *tableWidget = new QTableWidget(groupBox);
        (*tableWidget)->setColumnCount(5); // 移除状态列，因为每个表格只显示一种状态
        QStringList headers;
        headers << "滑槽号" << "车型代码" << "车型名称" << "数量" << "时间";
        (*tableWidget)->setHorizontalHeaderLabels(headers);
        (*tableWidget)->setAlternatingRowColors(true);
        (*tableWidget)->setSelectionBehavior(QAbstractItemView::SelectRows);
        (*tableWidget)->setSortingEnabled(false);
        (*tableWidget)->setEditTriggers(QAbstractItemView::NoEditTriggers);
        (*tableWidget)->horizontalHeader()->setStretchLastSection(true);
        (*tableWidget)->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        
        layout->addWidget(*tableWidget);
        return groupBox;
    };
    
    // 创建4个表格（2x2布局）
    QGroupBox* realTrayInGroupBox = createTableGroupBox(currentShiftTablePage, "实托盘搬入", &realTrayInTableWidget);
    QGroupBox* realTrayOutGroupBox = createTableGroupBox(currentShiftTablePage, "实托盘搬出", &realTrayOutTableWidget);
    QGroupBox* emptyTrayInGroupBox = createTableGroupBox(currentShiftTablePage, "空托盘搬入", &emptyTrayInTableWidget);
    QGroupBox* emptyTrayOutGroupBox = createTableGroupBox(currentShiftTablePage, "空托盘搬出", &emptyTrayOutTableWidget);
    
    // 将4个表格添加到网格布局（2x2）
    gridLayout->addWidget(realTrayInGroupBox, 0, 0);    // 左上：实托盘搬入
    gridLayout->addWidget(realTrayOutGroupBox, 0, 1);   // 右上：实托盘搬出
    gridLayout->addWidget(emptyTrayInGroupBox, 1, 0);   // 左下：空托盘搬入
    gridLayout->addWidget(emptyTrayOutGroupBox, 1, 1);  // 右下：空托盘搬出
    
    // 设置网格布局的拉伸比例，使表格平均分配空间
    gridLayout->setColumnStretch(0, 1);
    gridLayout->setColumnStretch(1, 1);
    gridLayout->setRowStretch(0, 1);
    gridLayout->setRowStretch(1, 1);
    
    currentShiftTableLayout->addLayout(gridLayout);
    
    // 注意：已移除清空和删除按钮，当前班次表格数据从数据表格自动加载
    
    // 保留旧的currentShiftTableWidget为nullptr，用于兼容性检查
    currentShiftTableWidget = nullptr;
    
    // 将当前班次表格页面插入到stackedWidget中（Index 1位置，在数据表格之前）
    ui->stackedWidget->insertWidget(1, currentShiftTablePage);
    
    // 将可视化记录页面插入到stackedWidget中（Index 3位置，因为插入了当前班次表格）
    // 工程组记录页面在Index 4
    // 原来的车型绑定页面会变成Index 5
    ui->stackedWidget->insertWidget(3, visualizationPage);
    
    // 创建工程组记录页面
    projectGroupPage = new QWidget();
    projectGroupPage->setObjectName("projectGroupPage");
    QVBoxLayout* projectGroupLayout = new QVBoxLayout(projectGroupPage);
    projectGroupLayout->setObjectName("projectGroupLayout");
    projectGroupLayout->setSpacing(10);
    projectGroupLayout->setContentsMargins(15, 15, 15, 15);  // 减少页面边距，增加可用空间
    
    // 创建统计区域GroupBox，填充整个工程组记录界面
    QGroupBox* projectGroupStatisticsBox = new QGroupBox(projectGroupPage);
    projectGroupStatisticsBox->setTitle("工程组统计");
    projectGroupStatisticsBox->setStyleSheet(
        "QGroupBox {"
        "font-size: 14pt;"
        "font-weight: bold;"
        "border: 2px solid #d0d0d0;"
        "border-radius: 8px;"
        "margin-top: 10px;"
        "padding-top: 15px;"
        "padding-bottom: 15px;"
        "}"
        "QGroupBox::title {"
        "subcontrol-origin: margin;"
        "left: 15px;"
        "padding: 0 8px 0 8px;"
        "}"
    );
    
    QVBoxLayout* projectGroupStatisticsLayout = new QVBoxLayout(projectGroupStatisticsBox);
    projectGroupStatisticsLayout->setSpacing(10);
    projectGroupStatisticsLayout->setContentsMargins(15, 15, 15, 15);  // 减少边距，增加可用空间
    
    // 创建表格
    projectGroupTable = new QTableWidget(projectGroupStatisticsBox);
    projectGroupTable->setColumnCount(5);
    QStringList headers;
    headers << "车型名称" << "实托盘搬入" << "实托盘搬出" << "空托盘搬入" << "空托盘搬出";
    projectGroupTable->setHorizontalHeaderLabels(headers);
    projectGroupTable->setAlternatingRowColors(true);
    projectGroupTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    projectGroupTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    projectGroupTable->horizontalHeader()->setStretchLastSection(true);
    projectGroupTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    
    // 让表格占据GroupBox内的所有可用空间
    projectGroupStatisticsLayout->addWidget(projectGroupTable, 1);
    
    // 创建水平布局用于放置班次按钮（右下角）
    QHBoxLayout* shiftButtonLayout = new QHBoxLayout();
    shiftButtonLayout->setContentsMargins(0, 5, 0, 5); // 顶部和底部都留5px间距
    shiftButtonLayout->setSpacing(0);
    shiftButtonLayout->addStretch(); // 左侧弹性空间，使按钮靠右
    
    // 创建班次显示按钮
    projectGroupShiftButton = new QPushButton(projectGroupStatisticsBox);
    QString currentShift = getCurrentShift();
    projectGroupShiftButton->setText(currentShift);
    projectGroupShiftButton->setMinimumSize(80, 35); // 增加高度以确保文字完整显示
    projectGroupShiftButton->setMaximumHeight(35); // 增加最大高度
    projectGroupShiftButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed); // 固定大小策略
    projectGroupShiftButton->setVisible(true); // 确保按钮可见
    projectGroupShiftButton->setStyleSheet(
        "QPushButton {"
        "font-size: 10pt;"
        "font-weight: bold;"
        "padding: 8px 12px;"
        "min-width: 80px;"
        "min-height: 35px;"
        "border: 2px solid #4CAF50;"
        "border-radius: 4px;"
        "background-color: #E8F5E9;"
        "color: #2E7D32;"
        "}"
        "QPushButton:hover {"
        "background-color: #C8E6C9;"
        "border: 2px solid #388E3C;"
        "}"
        "QPushButton:pressed {"
        "background-color: #A5D6A7;"
        "}"
    );
    shiftButtonLayout->addWidget(projectGroupShiftButton);
    
    // 连接班次按钮点击信号
    connect(projectGroupShiftButton, &QPushButton::clicked, this, &tcpClient::onProjectGroupShiftButtonClicked);
    
    // 创建一个容器widget来包含按钮布局，确保按钮有固定高度
    // 按钮实际高度 = 35px(内容) + 16px(上下padding) + 4px(上下border) = 55px
    // 加上布局的上下边距各5px，容器高度至少需要65px
    QWidget* shiftButtonWidget = new QWidget(projectGroupStatisticsBox);
    shiftButtonWidget->setLayout(shiftButtonLayout);
    shiftButtonWidget->setMinimumHeight(60); // 增加容器高度以确保按钮完整显示，包括padding和border
    shiftButtonWidget->setMaximumHeight(60);
    shiftButtonWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed); // 水平扩展，垂直固定
    shiftButtonWidget->setVisible(true); // 确保容器可见
    
    // 添加按钮容器到布局（在表格下方）
    projectGroupStatisticsLayout->addWidget(shiftButtonWidget, 0); // 不设置拉伸因子，保持固定高度，放在表格下方
    
    // 设置统计区域可以扩展，占据更多空间
    projectGroupLayout->addWidget(projectGroupStatisticsBox, 1);  // 使用拉伸因子1，让GroupBox占据可用空间
    
    // 将工程组记录页面插入到stackedWidget中（Index 4位置，因为插入了当前班次表格）
    ui->stackedWidget->insertWidget(4, projectGroupPage);
    
    // 创建总成指示表页面
    assemblyIndicatorPage = new QWidget();
    assemblyIndicatorPage->setObjectName("assemblyIndicatorPage");
    QVBoxLayout* assemblyIndicatorLayout = new QVBoxLayout(assemblyIndicatorPage);
    assemblyIndicatorLayout->setObjectName("assemblyIndicatorLayout");
    assemblyIndicatorLayout->setSpacing(10);
    assemblyIndicatorLayout->setContentsMargins(15, 15, 15, 15);
    
    // 创建总成指示表GroupBox
    QGroupBox* assemblyIndicatorBox = new QGroupBox(assemblyIndicatorPage);
    assemblyIndicatorBox->setTitle("总成指示表");
    assemblyIndicatorBox->setStyleSheet(
        "QGroupBox {"
        "font-size: 14pt;"
        "font-weight: bold;"
        "border: 2px solid #d0d0d0;"
        "border-radius: 8px;"
        "margin-top: 10px;"
        "padding-top: 15px;"
        "padding-bottom: 15px;"
        "}"
        "QGroupBox::title {"
        "subcontrol-origin: margin;"
        "left: 15px;"
        "padding: 0 8px 0 8px;"
        "}"
    );
    
    QVBoxLayout* assemblyIndicatorTableLayout = new QVBoxLayout(assemblyIndicatorBox);
    assemblyIndicatorTableLayout->setSpacing(10);
    assemblyIndicatorTableLayout->setContentsMargins(15, 15, 15, 15);
    
    // 创建总成指示表表格
    assemblyIndicatorTable = new QTableWidget(assemblyIndicatorBox);
    // 设置列数：车型名称，收容数，产量，生产总托数，节拍，托盘搬运，以及33列时间（8个时间列每个细分为4列 + 1个休息列1列）
    assemblyIndicatorTable->setColumnCount(39);
    
    // 创建自定义二级表头
    TwoLevelHeaderView* header = new TwoLevelHeaderView(Qt::Horizontal, assemblyIndicatorTable);
    assemblyIndicatorTable->setHorizontalHeader(header);
    
    // 设置基础列的二级表头（单级表头）
    header->setSecondLevelHeader(0, "车型名称");
    header->setSecondLevelHeader(1, "收容数");
    header->setSecondLevelHeader(2, "产量");
    header->setSecondLevelHeader(3, "生产总托数");
    header->setSecondLevelHeader(4, "节拍");
    header->setSecondLevelHeader(5, "托盘搬运");
    
    // 设置时间列的一级和二级表头
    // 9个时间列，每个细分为4列（15、30、45、60）
    QStringList timeHeaders = {
        "7:30-8:30\n17:15-18:15",
        "8:30-9:30\n18:15-19:15",
        "9:40-10:40\n19:25-20:25",
        "10:40-11:30\n20:25-21:15",
        "休息列",
        "12:15-13:15\n22:00-23:00",
        "13:15-14:15\n23:00-00:00",
        "14:25-15:25\n00:15-01:15",
        "15:25-16:15\n01:15-02:05"
    };
    
    int baseCol = 6; // 时间列从第6列开始
    // 设置9个时间列组的一级和二级表头
    int currentCol = baseCol; // 当前列索引
    int secondLevelValue = 15; // 二级表头起始值，从15开始
    int secondLevelCount = 0; // 二级表头计数器，用于计算最后一个
    
    // 先计算总共有多少个二级表头（8个时间列组 × 4列 = 32个）
    int totalSecondLevelHeaders = 8 * 4;
    
    for (int i = 0; i < 9; ++i) {
        // 设置一级表头文本（一级表头索引从0开始）
        header->setFirstLevelHeader(i, timeHeaders[i]);
        
        // 设置二级表头，并建立列索引到一级表头索引的映射
        // 休息列（i==4）只需要1列，不需要二级表头
        if (i == 4) {
            // 休息列：只有1列，只设置一级表头映射，不设置二级表头文本
            header->setSectionToFirstLevel(currentCol, i);
            // 不设置二级表头文本，这样就不会显示二级表头
            currentCol += 1; // 休息列只占1列
        } else {
            // 其他时间列：设置二级表头，每个时间列4列
            for (int j = 0; j < 4; ++j) {
                secondLevelCount++;
                // 设置当前值
                header->setSecondLevelHeader(currentCol, QString::number(secondLevelValue));
                
                // 判断是否是最后一个二级表头
                if (secondLevelCount == totalSecondLevelHeaders) {
                    // 最后一个：只加5，不加15（已经显示当前值，不需要更新secondLevelValue）
                } else {
                    // 判断是否是225，如果是则下一列加5
                    if (secondLevelValue == 225) {
                        secondLevelValue += 5; // 225的下一列：225 + 5 = 230
                    } else if (secondLevelCount == totalSecondLevelHeaders - 1) {
                        // 倒数第二个：加5（为最后一个做准备）
                        secondLevelValue += 5;
                    } else {
                        secondLevelValue += 15; // 其他每个加15
                    }
                }
                // 建立映射：列索引 -> 一级表头索引
                header->setSectionToFirstLevel(currentCol, i);
                currentCol += 1;
            }
        }
    }
    
    assemblyIndicatorTable->setAlternatingRowColors(true);
    assemblyIndicatorTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    assemblyIndicatorTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked); // 允许双击编辑
    
    // 设置默认行高（每行数据会分成2行记录显示）
    assemblyIndicatorTable->verticalHeader()->setDefaultSectionSize(30); // 默认行高30px
    assemblyIndicatorTable->verticalHeader()->setMinimumSectionSize(30);
    
    // 设置表头高度为原来的2倍（默认约30px，设置为80px，因为需要显示两级表头）
    header->setMinimumHeight(80);
    header->setDefaultSectionSize(80);
    header->setFixedHeight(80); // 固定高度确保二级表头有足够空间
    
    // 设置表头文本换行显示（通过样式表实现，支持换行符\n）
    header->setStyleSheet(
        "QHeaderView::section {"
        "padding: 5px;"
        "text-align: center;"
        "word-wrap: break-word;"
        "}"
    );
    
    // 强制刷新表头以确保二级表头正确显示
    header->update();
    header->updateGeometry();
    header->repaint(); // 强制重绘
    
    // 设置列宽模式：收容数、产量、生产总托数、节拍、托盘搬运这几列固定宽度，其他列自适应
    // 列0: 车型名称 - 固定宽度，增加宽度以完整显示车型名称
    header->setSectionResizeMode(0, QHeaderView::Fixed);
    assemblyIndicatorTable->setColumnWidth(0, 120);
    
    // 列1: 收容数 - 固定宽度，显示最多3个字
    header->setSectionResizeMode(1, QHeaderView::Fixed);
    assemblyIndicatorTable->setColumnWidth(1, 60);
    
    // 列2: 产量 - 固定宽度，显示最多3个字
    header->setSectionResizeMode(2, QHeaderView::Fixed);
    assemblyIndicatorTable->setColumnWidth(2, 60);
    
    // 列3: 生产总托数 - 固定宽度，增加宽度以完整显示
    header->setSectionResizeMode(3, QHeaderView::Fixed);
    assemblyIndicatorTable->setColumnWidth(3, 90);
    
    // 列4: 节拍 - 固定宽度，显示最多3个字
    header->setSectionResizeMode(4, QHeaderView::Fixed);
    assemblyIndicatorTable->setColumnWidth(4, 60);
    
    // 列5: 托盘搬运 - 固定宽度，显示最多3个字（需要换行）
    header->setSectionResizeMode(5, QHeaderView::Fixed);
    assemblyIndicatorTable->setColumnWidth(5, 60);
    
    // 列6-38: 33列时间列（8个时间列每个细分为4列 + 1个休息列1列）
    // 休息列（列22）设置为固定宽度，刚好放下3个字
    // 其他时间列自适应
    for (int i = 6; i <= 38; ++i) {
        // 休息列：列22（baseCol + 4*4 = 22，因为前4个时间列各占4列）
        if (i == 22) {
            header->setSectionResizeMode(i, QHeaderView::Fixed);
            assemblyIndicatorTable->setColumnWidth(i, 60);
        } else {
            header->setSectionResizeMode(i, QHeaderView::Stretch);
        }
    }
    
    // 设置表格单元格文本换行显示（对于固定宽度的列）
    assemblyIndicatorTable->setWordWrap(true);
    
    // 设置表格样式，确保从托盘搬运列开始的单元格能够正确显示上下两行
    // 保留边框线，但设置网格线颜色与背景色匹配以实现颜色串联效果
    assemblyIndicatorTable->setStyleSheet(
        "QTableWidget {"
        "border: 1px solid #dee2e6;"
        "gridline-color: #dee2e6;"
        "}"
        "QTableWidget::item {"
        "padding: 2px;"
        "}"
    );
    assemblyIndicatorTable->setShowGrid(true); // 显示网格线
    
    // 让表格占据GroupBox内的所有可用空间
    assemblyIndicatorTableLayout->addWidget(assemblyIndicatorTable, 1);
    
    // 设置总成指示表区域可以扩展，占据更多空间
    assemblyIndicatorLayout->addWidget(assemblyIndicatorBox, 1);
    
    // 将总成指示表页面插入到stackedWidget中（Index 5位置，在工程组记录之后）
    ui->stackedWidget->insertWidget(5, assemblyIndicatorPage);
    
    // 连接界面切换按钮信号槽
    connect(ui->pushButtonConnectionPage, &QPushButton::clicked, this, &tcpClient::onConnectionPageClicked);
    connect(pushButtonCurrentShiftTablePage, &QPushButton::clicked, this, &tcpClient::onCurrentShiftTablePageClicked);
    connect(ui->pushButtonTablePage, &QPushButton::clicked, this, &tcpClient::onTablePageClicked);
    connect(pushButtonVisualizationPage, &QPushButton::clicked, this, &tcpClient::onVisualizationPageClicked);
    connect(pushButtonProjectGroupPage, &QPushButton::clicked, this, &tcpClient::onProjectGroupPageClicked);
    connect(pushButtonAssemblyIndicatorPage, &QPushButton::clicked, this, &tcpClient::onAssemblyIndicatorPageClicked);
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
    
    // 在PLC连接设置区域添加保存按钮
    pushButtonSavePlcConfig = new QPushButton(ui->groupBoxConnection);
    pushButtonSavePlcConfig->setText("保存配置");
    pushButtonSavePlcConfig->setObjectName("pushButtonSavePlcConfig");
    ui->horizontalLayout->addWidget(pushButtonSavePlcConfig);
    connect(pushButtonSavePlcConfig, &QPushButton::clicked, this, &tcpClient::onSavePlcConnectionConfigClicked);
    
    // 创建服务端连接设置区域
    groupBoxServerConnection = new QGroupBox(ui->connectionPage);
    groupBoxServerConnection->setTitle("服务端连接设置");
    groupBoxServerConnection->setObjectName("groupBoxServerConnection");
    
    QHBoxLayout* serverConnectionLayout = new QHBoxLayout(groupBoxServerConnection);
    serverConnectionLayout->setObjectName("serverConnectionLayout");
    
    QLabel* labelServerIP = new QLabel(groupBoxServerConnection);
    labelServerIP->setText("服务器IP:");
    serverConnectionLayout->addWidget(labelServerIP);
    
    lineEditServerIP = new QLineEdit(groupBoxServerConnection);
    lineEditServerIP->setMinimumSize(120, 0);
    lineEditServerIP->setPlaceholderText("服务端IP地址");
    serverConnectionLayout->addWidget(lineEditServerIP);
    
    QLabel* labelServerPort = new QLabel(groupBoxServerConnection);
    labelServerPort->setText("端口:");
    serverConnectionLayout->addWidget(labelServerPort);
    
    lineEditServerPort = new QLineEdit(groupBoxServerConnection);
    lineEditServerPort->setMinimumSize(80, 0);
    lineEditServerPort->setPlaceholderText("端口号");
    serverConnectionLayout->addWidget(lineEditServerPort);
    
    pushButtonServerConnect = new QPushButton(groupBoxServerConnection);
    pushButtonServerConnect->setText("连接");
    pushButtonServerConnect->setObjectName("pushButtonServerConnect");
    serverConnectionLayout->addWidget(pushButtonServerConnect);
    
    pushButtonServerDisconnect = new QPushButton(groupBoxServerConnection);
    pushButtonServerDisconnect->setText("断开");
    pushButtonServerDisconnect->setObjectName("pushButtonServerDisconnect");
    pushButtonServerDisconnect->setEnabled(false);
    serverConnectionLayout->addWidget(pushButtonServerDisconnect);
    
    pushButtonSaveServerConfig = new QPushButton(groupBoxServerConnection);
    pushButtonSaveServerConfig->setText("保存配置");
    pushButtonSaveServerConfig->setObjectName("pushButtonSaveServerConfig");
    serverConnectionLayout->addWidget(pushButtonSaveServerConfig);
    
    // 创建服务端连接状态标签
    labelServerConnectionStatus = new QLabel(groupBoxServerConnection);
    labelServerConnectionStatus->setText("未连接");
    labelServerConnectionStatus->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    serverConnectionLayout->addWidget(labelServerConnectionStatus);
    
    QSpacerItem* serverSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);
    serverConnectionLayout->addItem(serverSpacer);
    
    // 将服务端连接设置插入到PLC连接设置之后
    ui->verticalLayout_2->insertWidget(1, groupBoxServerConnection);
    
    // 连接服务端连接配置按钮信号槽
    connect(pushButtonSaveServerConfig, &QPushButton::clicked, this, &tcpClient::onSaveServerConnectionConfigClicked);
    connect(pushButtonServerConnect, &QPushButton::clicked, this, &tcpClient::onServerConnectClicked);
    connect(pushButtonServerDisconnect, &QPushButton::clicked, this, &tcpClient::onServerDisconnectClicked);
    
    // 创建ED软件连接设置区域
    groupBoxEdSoftwareConnection = new QGroupBox(ui->connectionPage);
    groupBoxEdSoftwareConnection->setTitle("ED软件连接设置");
    groupBoxEdSoftwareConnection->setObjectName("groupBoxEdSoftwareConnection");
    
    QHBoxLayout* edSoftwareConnectionLayout = new QHBoxLayout(groupBoxEdSoftwareConnection);
    edSoftwareConnectionLayout->setObjectName("edSoftwareConnectionLayout");
    
    QLabel* labelEdSoftwareIP = new QLabel(groupBoxEdSoftwareConnection);
    labelEdSoftwareIP->setText("服务器IP:");
    edSoftwareConnectionLayout->addWidget(labelEdSoftwareIP);
    
    lineEditEdSoftwareIP = new QLineEdit(groupBoxEdSoftwareConnection);
    lineEditEdSoftwareIP->setMinimumSize(120, 0);
    lineEditEdSoftwareIP->setPlaceholderText("ED软件IP地址");
    edSoftwareConnectionLayout->addWidget(lineEditEdSoftwareIP);
    
    QLabel* labelEdSoftwarePort = new QLabel(groupBoxEdSoftwareConnection);
    labelEdSoftwarePort->setText("端口:");
    edSoftwareConnectionLayout->addWidget(labelEdSoftwarePort);
    
    lineEditEdSoftwarePort = new QLineEdit(groupBoxEdSoftwareConnection);
    lineEditEdSoftwarePort->setMinimumSize(80, 0);
    lineEditEdSoftwarePort->setPlaceholderText("端口号");
    edSoftwareConnectionLayout->addWidget(lineEditEdSoftwarePort);
    
    pushButtonEdSoftwareConnect = new QPushButton(groupBoxEdSoftwareConnection);
    pushButtonEdSoftwareConnect->setText("连接");
    pushButtonEdSoftwareConnect->setObjectName("pushButtonEdSoftwareConnect");
    edSoftwareConnectionLayout->addWidget(pushButtonEdSoftwareConnect);
    
    pushButtonEdSoftwareDisconnect = new QPushButton(groupBoxEdSoftwareConnection);
    pushButtonEdSoftwareDisconnect->setText("断开");
    pushButtonEdSoftwareDisconnect->setObjectName("pushButtonEdSoftwareDisconnect");
    pushButtonEdSoftwareDisconnect->setEnabled(false);
    edSoftwareConnectionLayout->addWidget(pushButtonEdSoftwareDisconnect);
    
    pushButtonSaveEdSoftwareConfig = new QPushButton(groupBoxEdSoftwareConnection);
    pushButtonSaveEdSoftwareConfig->setText("保存配置");
    pushButtonSaveEdSoftwareConfig->setObjectName("pushButtonSaveEdSoftwareConfig");
    edSoftwareConnectionLayout->addWidget(pushButtonSaveEdSoftwareConfig);
    
    // 创建ED软件连接状态标签
    labelEdSoftwareConnectionStatus = new QLabel(groupBoxEdSoftwareConnection);
    labelEdSoftwareConnectionStatus->setText("未连接");
    labelEdSoftwareConnectionStatus->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    edSoftwareConnectionLayout->addWidget(labelEdSoftwareConnectionStatus);
    
    QSpacerItem* edSoftwareSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);
    edSoftwareConnectionLayout->addItem(edSoftwareSpacer);
    
    // 将ED软件连接设置插入到服务端连接设置之后
    ui->verticalLayout_2->insertWidget(2, groupBoxEdSoftwareConnection);
    
    // 连接ED软件连接配置按钮信号槽
    connect(pushButtonSaveEdSoftwareConfig, &QPushButton::clicked, this, &tcpClient::onSaveEdSoftwareConnectionConfigClicked);
    connect(pushButtonEdSoftwareConnect, &QPushButton::clicked, this, &tcpClient::onEdSoftwareConnectClicked);
    connect(pushButtonEdSoftwareDisconnect, &QPushButton::clicked, this, &tcpClient::onEdSoftwareDisconnectClicked);
    
    // 创建班次设置区域
    groupBoxShiftConfig = new QGroupBox(ui->connectionPage);
    groupBoxShiftConfig->setTitle("班次设置");
    groupBoxShiftConfig->setObjectName("groupBoxShiftConfig");
    
    QGridLayout* shiftConfigLayout = new QGridLayout(groupBoxShiftConfig);
    shiftConfigLayout->setObjectName("shiftConfigLayout");
    shiftConfigLayout->setSpacing(10);
    shiftConfigLayout->setContentsMargins(15, 15, 15, 15);
    
    // 白班设置
    QLabel* labelDayShift = new QLabel(groupBoxShiftConfig);
    labelDayShift->setText("白班:");
    labelDayShift->setStyleSheet("font-weight: bold;");
    shiftConfigLayout->addWidget(labelDayShift, 0, 0);
    
    QLabel* labelDayShiftStart = new QLabel(groupBoxShiftConfig);
    labelDayShiftStart->setText("开始时间:");
    shiftConfigLayout->addWidget(labelDayShiftStart, 0, 1);
    
    timeEditDayShiftStart = new QTimeEdit(groupBoxShiftConfig);
    timeEditDayShiftStart->setDisplayFormat("HH:mm");
    timeEditDayShiftStart->setTime(QTime(7, 15, 0)); // 默认7:15
    timeEditDayShiftStart->setMinimumSize(100, 0);
    shiftConfigLayout->addWidget(timeEditDayShiftStart, 0, 2);
    
    QLabel* labelDayShiftEnd = new QLabel(groupBoxShiftConfig);
    labelDayShiftEnd->setText("结束时间:");
    shiftConfigLayout->addWidget(labelDayShiftEnd, 0, 3);
    
    timeEditDayShiftEnd = new QTimeEdit(groupBoxShiftConfig);
    timeEditDayShiftEnd->setDisplayFormat("HH:mm");
    timeEditDayShiftEnd->setTime(QTime(17, 30, 0)); // 默认17:30
    timeEditDayShiftEnd->setMinimumSize(100, 0);
    shiftConfigLayout->addWidget(timeEditDayShiftEnd, 0, 4);
    
    // 夜班设置
    QLabel* labelNightShift = new QLabel(groupBoxShiftConfig);
    labelNightShift->setText("夜班:");
    labelNightShift->setStyleSheet("font-weight: bold;");
    shiftConfigLayout->addWidget(labelNightShift, 1, 0);
    
    QLabel* labelNightShiftStart = new QLabel(groupBoxShiftConfig);
    labelNightShiftStart->setText("开始时间:");
    shiftConfigLayout->addWidget(labelNightShiftStart, 1, 1);
    
    timeEditNightShiftStart = new QTimeEdit(groupBoxShiftConfig);
    timeEditNightShiftStart->setDisplayFormat("HH:mm");
    timeEditNightShiftStart->setTime(QTime(17, 30, 0)); // 默认17:30
    timeEditNightShiftStart->setMinimumSize(100, 0);
    shiftConfigLayout->addWidget(timeEditNightShiftStart, 1, 2);
    
    QLabel* labelNightShiftEnd = new QLabel(groupBoxShiftConfig);
    labelNightShiftEnd->setText("结束时间:");
    shiftConfigLayout->addWidget(labelNightShiftEnd, 1, 3);
    
    timeEditNightShiftEnd = new QTimeEdit(groupBoxShiftConfig);
    timeEditNightShiftEnd->setDisplayFormat("HH:mm");
    timeEditNightShiftEnd->setTime(QTime(7, 15, 0)); // 默认次日7:15
    timeEditNightShiftEnd->setMinimumSize(100, 0);
    shiftConfigLayout->addWidget(timeEditNightShiftEnd, 1, 4);
    
    // 保存按钮
    pushButtonSaveShiftConfig = new QPushButton(groupBoxShiftConfig);
    pushButtonSaveShiftConfig->setText("保存设置");
    pushButtonSaveShiftConfig->setObjectName("pushButtonSaveShiftConfig");
    shiftConfigLayout->addWidget(pushButtonSaveShiftConfig, 2, 0, 1, 5);
    
    // 将班次设置插入到ED软件连接设置之后
    ui->verticalLayout_2->insertWidget(3, groupBoxShiftConfig);
    
    // 连接班次设置按钮信号槽
    connect(pushButtonSaveShiftConfig, &QPushButton::clicked, this, &tcpClient::onSaveShiftConfigClicked);
    
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
    ui->stackedWidget->setCurrentIndex(2);  // 索引从1变为2，因为插入了当前班次表格
    ui->pushButtonConnectionPage->setChecked(false);
    pushButtonCurrentShiftTablePage->setChecked(false);
    ui->pushButtonTablePage->setChecked(true);
    pushButtonVisualizationPage->setChecked(false);
    pushButtonProjectGroupPage->setChecked(false);
    pushButtonAssemblyIndicatorPage->setChecked(false);
    ui->pushButtonVehicleBindingPage->setChecked(false);

    ui->pushButtonExportTable->setVisible(false);
    // 隐藏上方状态标签
    ui->labelStatus->setVisible(false);
    // 右下角连接状态标签（3个TCP连接状态）
    // 创建水平布局容器，用于右对齐
    QHBoxLayout* statusLayout = new QHBoxLayout();
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->setSpacing(10);
    
    // 添加弹性空间，使标签靠右
    statusLayout->addStretch();
    
    // PLC连接状态标签
    labelConnectionStatus = new QLabel(this);
    labelConnectionStatus->setText("PLC: 未连接");
    labelConnectionStatus->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    labelConnectionStatus->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    statusLayout->addWidget(labelConnectionStatus);
    
    // 服务端连接状态标签
    QLabel* labelServerStatus = new QLabel(this);
    labelServerStatus->setText("服务端: 未连接");
    labelServerStatus->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    labelServerStatus->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    statusLayout->addWidget(labelServerStatus);
    m_labelServerStatus = labelServerStatus; // 保存引用
    
    // ED软件连接状态标签
    QLabel* labelEdSoftwareStatus = new QLabel(this);
    labelEdSoftwareStatus->setText("ED软件: 未连接");
    labelEdSoftwareStatus->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    labelEdSoftwareStatus->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    statusLayout->addWidget(labelEdSoftwareStatus);
    m_labelEdSoftwareStatus = labelEdSoftwareStatus; // 保存引用
    // 浣跨敤Qt榛樿鏍峰紡`r`n    labelConnectionStatus->setStyleSheet("");
    // 将状态布局添加到主布局底部
    ui->verticalLayout->addLayout(statusLayout);

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
            pushButtonVisualizationPage->setStyleSheet("");
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
        QPushButton#pushButtonVisualizationPage,
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
        QPushButton#pushButtonVisualizationPage:hover,
        QPushButton#pushButtonProjectGroupPage:hover,
        QPushButton#pushButtonAssemblyIndicatorPage:hover,
        QPushButton#pushButtonVehicleBindingPage:hover {
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                                      stop: 0 #5d6d7e, stop: 1 #34495e);
        }

        QPushButton#pushButtonConnectionPage:checked,
        QPushButton#pushButtonTablePage:checked,
        QPushButton#pushButtonVisualizationPage:checked,
        QPushButton#pushButtonProjectGroupPage:checked,
        QPushButton#pushButtonAssemblyIndicatorPage:checked,
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
    ui->tableWidget->setSortingEnabled(false); // 禁用排序，改为筛选功能

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

    // 连接表头点击信号，实现筛选功能
    connect(ui->tableWidget->horizontalHeader(), &QHeaderView::sectionClicked, this, &tcpClient::onTableHeaderClicked);
    
    // 延迟记录列宽设置到日志，确保UI已准备好
    QTimer::singleShot(200, this, [this]() {
        appendToLog("数据表格列宽设置完成：滑槽号(80px), 状态(120px), 代码(100px), 名称(120px), 数量(100px), 时间(自适应)");
        appendToLog("数据表格已设置为只读模式，防止数据被误修改");
        appendToLog("点击表头可进行筛选操作");
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
            // 先取消所有按钮的选中状态，避免出现多个选中状态
            ui->pushButtonConnectionPage->setChecked(false);
            pushButtonCurrentShiftTablePage->setChecked(false);
            ui->pushButtonTablePage->setChecked(false);
            pushButtonVisualizationPage->setChecked(false);
            pushButtonProjectGroupPage->setChecked(false);
            pushButtonAssemblyIndicatorPage->setChecked(false);
            ui->pushButtonVehicleBindingPage->setChecked(false);
            // 然后设置正确的按钮为选中状态
            ui->pushButtonTablePage->setChecked(true);
            return;
        }
    }

    ui->stackedWidget->setCurrentIndex(0);
    ui->pushButtonConnectionPage->setChecked(true);
    pushButtonCurrentShiftTablePage->setChecked(false);
    ui->pushButtonTablePage->setChecked(false);
    pushButtonVisualizationPage->setChecked(false);
    pushButtonProjectGroupPage->setChecked(false);
    pushButtonAssemblyIndicatorPage->setChecked(false);
    ui->pushButtonVehicleBindingPage->setChecked(false);
}

/**
 * @brief 切换到当前班次表格界面
 */
void tcpClient::onCurrentShiftTablePageClicked()
{
    ui->stackedWidget->setCurrentIndex(1);
    ui->pushButtonConnectionPage->setChecked(false);
    pushButtonCurrentShiftTablePage->setChecked(true);
    ui->pushButtonTablePage->setChecked(false);
    pushButtonVisualizationPage->setChecked(false);
    pushButtonProjectGroupPage->setChecked(false);
    pushButtonAssemblyIndicatorPage->setChecked(false);
    ui->pushButtonVehicleBindingPage->setChecked(false);
}

/**
 * @brief 切换到表格界面
 */
void tcpClient::onTablePageClicked()
{
    ui->stackedWidget->setCurrentIndex(2);  // 索引从1变为2，因为插入了当前班次表格
    ui->pushButtonConnectionPage->setChecked(false);
    pushButtonCurrentShiftTablePage->setChecked(false);
    ui->pushButtonTablePage->setChecked(true);
    pushButtonVisualizationPage->setChecked(false);
    pushButtonProjectGroupPage->setChecked(false);
    pushButtonAssemblyIndicatorPage->setChecked(false);
    ui->pushButtonVehicleBindingPage->setChecked(false);
}

/**
 * @brief 切换到可视化记录界面
 */
void tcpClient::onVisualizationPageClicked()
{
    ui->stackedWidget->setCurrentIndex(3);  // 索引从2变为3，因为插入了当前班次表格
    ui->pushButtonConnectionPage->setChecked(false);
    pushButtonCurrentShiftTablePage->setChecked(false);
    ui->pushButtonTablePage->setChecked(false);
    pushButtonVisualizationPage->setChecked(true);
    pushButtonProjectGroupPage->setChecked(false);
    pushButtonAssemblyIndicatorPage->setChecked(false);
    ui->pushButtonVehicleBindingPage->setChecked(false);
}

/**
 * @brief 切换到工程组记录界面
 */
void tcpClient::onProjectGroupPageClicked()
{
    ui->stackedWidget->setCurrentIndex(4);  // 索引从3变为4，因为插入了当前班次表格
    ui->pushButtonConnectionPage->setChecked(false);
    pushButtonCurrentShiftTablePage->setChecked(false);
    ui->pushButtonTablePage->setChecked(false);
    pushButtonVisualizationPage->setChecked(false);
    pushButtonProjectGroupPage->setChecked(true);
    pushButtonAssemblyIndicatorPage->setChecked(false);
    ui->pushButtonVehicleBindingPage->setChecked(false);
    
    // 更新班次按钮文本
    if (projectGroupShiftButton) {
        QString currentShift = getCurrentShift();
        projectGroupShiftButton->setText(currentShift);
    }
    
    // 重置为显示当前班次
    m_projectGroupDisplayShift = "current";
    
    // 更新工程组统计表格
    updateProjectGroupStatistics();
}

/**
 * @brief 切换到总成指示表界面
 */
void tcpClient::onAssemblyIndicatorPageClicked()
{
    ui->stackedWidget->setCurrentIndex(5);  // 总成指示表在索引5
    ui->pushButtonConnectionPage->setChecked(false);
    pushButtonCurrentShiftTablePage->setChecked(false);
    ui->pushButtonTablePage->setChecked(false);
    pushButtonVisualizationPage->setChecked(false);
    pushButtonProjectGroupPage->setChecked(false);
    pushButtonAssemblyIndicatorPage->setChecked(true);
    ui->pushButtonVehicleBindingPage->setChecked(false);
}

/**
 * @brief 切换到车型绑定界面
 */
void tcpClient::onVehicleBindingPageClicked()
{
    // 检查是否需要密码验证
    if (m_isPasswordSet) {
        if (!showPasswordDialog("密码验证", "请输入密码以进入车型绑定界面:")) {
            // 密码验证失败或取消，恢复按钮状态
            // 先取消所有按钮的选中状态，避免出现多个选中状态
            ui->pushButtonConnectionPage->setChecked(false);
            pushButtonCurrentShiftTablePage->setChecked(false);
            ui->pushButtonTablePage->setChecked(false);
            pushButtonVisualizationPage->setChecked(false);
            pushButtonProjectGroupPage->setChecked(false);
            pushButtonAssemblyIndicatorPage->setChecked(false);
            ui->pushButtonVehicleBindingPage->setChecked(false);
            // 恢复到数据表格页面
            ui->stackedWidget->setCurrentIndex(2);  // 索引从1变为2，因为插入了当前班次表格
            ui->pushButtonTablePage->setChecked(true);
            return;
        }
    }
    
    ui->stackedWidget->setCurrentIndex(6);  // 更新索引：当前班次表格在Index 1，数据表格在Index 2，可视化记录在Index 3，工程组记录在Index 4，总成指示表在Index 5，车型绑定在Index 6
    ui->pushButtonConnectionPage->setChecked(false);
    pushButtonCurrentShiftTablePage->setChecked(false);
    ui->pushButtonTablePage->setChecked(false);
    pushButtonVisualizationPage->setChecked(false);
    pushButtonProjectGroupPage->setChecked(false);
    pushButtonAssemblyIndicatorPage->setChecked(false);
    ui->pushButtonVehicleBindingPage->setChecked(true);
}

/**
 * @brief 清空表格按钮点击处理
 */
void tcpClient::onClearTableClicked()
{
    // 检查是否需要密码验证
    if (m_isPasswordSet) {
        if (!showPasswordDialog("密码验证", "请输入密码以清空表格:")) {
            return;
        }
    }
    
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
    // 检查是否需要密码验证
    if (m_isPasswordSet) {
        if (!showPasswordDialog("密码验证", "请输入密码以删除记录:")) {
            return;
        }
    }
    
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
 * @brief 清空当前班次表格按钮点击处理
 */
void tcpClient::onClearCurrentShiftTableClicked()
{
    // 检查是否需要密码验证
    if (m_isPasswordSet) {
        if (!showPasswordDialog("密码验证", "请输入密码以清空当前班次表格:")) {
            return;
        }
    }
    
    // 计算所有表格的总记录数
    int totalRowCount = 0;
    if (realTrayInTableWidget) totalRowCount += realTrayInTableWidget->rowCount();
    if (realTrayOutTableWidget) totalRowCount += realTrayOutTableWidget->rowCount();
    if (emptyTrayInTableWidget) totalRowCount += emptyTrayInTableWidget->rowCount();
    if (emptyTrayOutTableWidget) totalRowCount += emptyTrayOutTableWidget->rowCount();
    
    if (totalRowCount > 0) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "确认清空",
            QString("确定要清空所有当前班次表格中的 %1 条记录吗？").arg(totalRowCount),
            QMessageBox::Yes | QMessageBox::No
            );

        if (reply == QMessageBox::Yes) {
            // 清空所有4个表格（数据从 data_records 表加载，只需清空UI表格）
            if (realTrayInTableWidget) realTrayInTableWidget->setRowCount(0);
            if (realTrayOutTableWidget) realTrayOutTableWidget->setRowCount(0);
            if (emptyTrayInTableWidget) emptyTrayInTableWidget->setRowCount(0);
            if (emptyTrayOutTableWidget) emptyTrayOutTableWidget->setRowCount(0);
            
            appendToLog(QString("已清空所有当前班次表格，共 %1 条记录").arg(totalRowCount), false);
        }
    } else {
        appendToLog("当前班次表格为空，无需清空", false);
    }
}

/**
 * @brief 删除选中当前班次表格按钮点击处理
 */
void tcpClient::onDeleteCurrentShiftTableClicked()
{
    // 检查是否需要密码验证
    if (m_isPasswordSet) {
        if (!showPasswordDialog("密码验证", "请输入密码以删除记录:")) {
            return;
        }
    }
    
    // 查找哪个表格有选中的项
    QTableWidget* targetTable = nullptr;
    QList<QTableWidgetItem*> selectedItems;
    
    // 检查4个表格，找到有选中项的表格
    if (realTrayInTableWidget && !realTrayInTableWidget->selectedItems().isEmpty()) {
        targetTable = realTrayInTableWidget;
        selectedItems = realTrayInTableWidget->selectedItems();
    } else if (realTrayOutTableWidget && !realTrayOutTableWidget->selectedItems().isEmpty()) {
        targetTable = realTrayOutTableWidget;
        selectedItems = realTrayOutTableWidget->selectedItems();
    } else if (emptyTrayInTableWidget && !emptyTrayInTableWidget->selectedItems().isEmpty()) {
        targetTable = emptyTrayInTableWidget;
        selectedItems = emptyTrayInTableWidget->selectedItems();
    } else if (emptyTrayOutTableWidget && !emptyTrayOutTableWidget->selectedItems().isEmpty()) {
        targetTable = emptyTrayOutTableWidget;
        selectedItems = emptyTrayOutTableWidget->selectedItems();
    }
    
    if (!targetTable || selectedItems.isEmpty()) {
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
        // 根据目标表格确定状态
        QString status;
        if (targetTable == realTrayInTableWidget) {
            status = "实托盘搬入";
        } else if (targetTable == realTrayOutTableWidget) {
            status = "实托盘搬出";
        } else if (targetTable == emptyTrayInTableWidget) {
            status = "空托盘搬入";
        } else if (targetTable == emptyTrayOutTableWidget) {
            status = "空托盘搬出";
        }
        
        for (int row : rowsToDelete) {
            // 获取要删除的记录信息用于数据库删除
            // 列索引：0=滑槽号, 1=车型代码, 2=车型名称, 3=数量, 4=时间
            QString slotNo = targetTable->item(row, 0)->text();
            QString modelCode = targetTable->item(row, 1)->text();
            QString modelName = targetTable->item(row, 2)->text();
            QString count = targetTable->item(row, 3)->text();
            QString time = targetTable->item(row, 4)->text();

            // 从表格中删除
            targetTable->removeRow(row);

            // 从数据库中删除对应记录（从 data_records 表删除）
            deleteDataRecord(slotNo.toInt(), status, modelName, modelCode, count.toInt(), time);
        }

        appendToLog(QString("已删除 %1 条当前班次表格记录").arg(rowsToDelete.size()), false);
    }
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
    m_isAutoReconnecting = false; // 标记是手动连接
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
        // 用户手动断开连接，停止自动重连定时器
        m_plcAutoReconnectTimer->stop();
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
    m_plcAutoReconnectTimer->stop(); // 连接成功，停止自动重连定时器
    m_isConnected = true;
    updateConnectionStatus(true);

    // 连接成功才写入日志（不管是手动连接还是自动重连）
    QString successMsg = "连接成功！";
    appendToLog(successMsg, false);
    qInfo() << successMsg << "服务器地址:" << m_socket->peerAddress().toString() << "端口:" << m_socket->peerPort();
    
    m_isAutoReconnecting = false; // 重置标志
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
    
    // 启动自动重连定时器
    if (!m_plcAutoReconnectTimer->isActive()) {
        m_plcAutoReconnectTimer->start();
        // 自动重连不写入日志
    }
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

    // 如果是自动重连导致的错误，不写入日志；如果是手动连接导致的错误，写入日志
    if (!m_isAutoReconnecting) {
        QString errorMsg = m_socket->errorString();
        appendToLog(QString("连接错误: %1").arg(errorMsg), true);
    }
    
    // 启动自动重连定时器（用户主动取消连接通常不会触发errorOccurred信号）
    if (!m_plcAutoReconnectTimer->isActive()) {
        m_plcAutoReconnectTimer->start();
        // 自动重连不写入日志
    }
    
    m_isAutoReconnecting = false; // 重置标志
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
    
    // 如果是自动重连导致的超时，不写入日志；如果是手动连接导致的超时，写入日志
    if (!m_isAutoReconnecting) {
        appendToLog("连接超时", true);
    }
    
    updateConnectionStatus(false);
    
    // 启动自动重连定时器
    if (!m_plcAutoReconnectTimer->isActive()) {
        m_plcAutoReconnectTimer->start();
        // 自动重连不写入日志
    }
    
    m_isAutoReconnecting = false; // 重置标志
}

/**
 * @brief PLC自动重连处理
 */
void tcpClient::onPlcAutoReconnect()
{
    // 如果已经连接，停止自动重连定时器
    if (m_isConnected || m_socket->state() == QAbstractSocket::ConnectedState) {
        m_plcAutoReconnectTimer->stop();
        return;
    }
    
    // 如果正在连接中，不重复连接
    if (m_socket->state() == QAbstractSocket::ConnectingState) {
        return;
    }
    
    // 获取保存的PLC连接配置
    QString serverIP = ui->lineEditServerIP->text().trimmed();
    QString portStr = ui->lineEditPort->text().trimmed();
    
    // 验证IP地址和端口
    if (serverIP.isEmpty()) {
        qDebug() << "PLC自动重连: IP地址为空，跳过重连";
        return;
    }
    
    bool ok;
    int port = portStr.toInt(&ok);
    if (!ok || port <= 0 || port > 65535) {
        qDebug() << "PLC自动重连: 端口号无效，跳过重连";
        return;
    }
    
    // 尝试自动重连（不写入日志，连接成功时才写入日志）
    m_isAutoReconnecting = true; // 标记正在自动重连
    m_socket->connectToHost(serverIP, port);
    m_connectionTimer->start();
}

/**
 * @brief 当前班次表格每日清空处理（每天凌晨6点）
 */
void tcpClient::onCurrentShiftTableDailyClear()
{
    QDateTime now = QDateTime::currentDateTime();
    QDate today = now.date();
    QTime currentTime = now.time();
    
    // 检查是否是凌晨6点（6:00:00 - 6:00:59之间）
    if (currentTime.hour() == 6 && currentTime.minute() == 0) {
        // 检查今天是否已经清空过
        if (m_lastCurrentShiftTableClearDate != today) {
            // 清空当前班次表格（数据从 data_records 表加载，只需清空UI表格）
            if (currentShiftTableWidget) {
                int rowCount = currentShiftTableWidget->rowCount();
                if (rowCount > 0) {
                    currentShiftTableWidget->setRowCount(0);
                    // 同时清空所有4个表格
                    if (realTrayInTableWidget) realTrayInTableWidget->setRowCount(0);
                    if (realTrayOutTableWidget) realTrayOutTableWidget->setRowCount(0);
                    if (emptyTrayInTableWidget) emptyTrayInTableWidget->setRowCount(0);
                    if (emptyTrayOutTableWidget) emptyTrayOutTableWidget->setRowCount(0);
                    m_lastCurrentShiftTableClearDate = today;
                    appendToLog(QString("已自动清空当前班次表格（每天凌晨6点），共 %1 条记录").arg(rowCount), false);
                    qInfo() << "已自动清空当前班次表格（每天凌晨6点），共" << rowCount << "条记录";
                }
            }
        }
    }
}

/**
 * @brief 更新连接状态显示
 * @param connected 连接状态
 */
void tcpClient::updateConnectionStatus(bool connected)
{
    if (connected) {
        labelConnectionStatus->setText("PLC: 已连接");
        labelConnectionStatus->setStyleSheet("QLabel { color: green; font-weight: bold; }");
        // 浣跨敤Qt榛樿鏍峰紡`r`n        labelConnectionStatus->setStyleSheet("");
        ui->pushButtonConnect->setEnabled(false);
        ui->pushButtonDisconnect->setEnabled(true);
        ui->lineEditServerIP->setEnabled(false);
        ui->lineEditPort->setEnabled(false);
    } else {
        labelConnectionStatus->setText("PLC: 未连接");
        labelConnectionStatus->setStyleSheet("QLabel { color: red; font-weight: bold; }");
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
                QTableWidgetItem* codeItem = ui->tableWidgetVehicleBinding->item(row, 1);
                QTableWidgetItem* nameItem = ui->tableWidgetVehicleBinding->item(row, 2);
                QTableWidgetItem* countItem = ui->tableWidgetVehicleBinding->item(row, 3);
                
                // 检查表格项是否存在
                if (!codeItem || !nameItem || !countItem) {
                    qDebug() << "Table items not found for row:" << row;
                    continue; // 跳过这一行，继续查找下一行
                }
                
                QString code = codeItem->text().trimmed();
                if (modelCodeStr == code) {
                    vehicleCode = code;
                    vehicleName = nameItem->text();
                    count = countItem->text().toInt();
                    break;
                }
            }

            // 如果找到车型信息，添加到表格
            if (!vehicleName.isEmpty()) {
                int tableRow = ui->tableWidget->rowCount();
                ui->tableWidget->insertRow(tableRow);
                
                // 滑槽号（实托盘1-3显示为1-3，空托盘1-3显示为4-6）
                // 映射为实际滑槽号：1->2001, 2->2002, 3->2003, 4->2103, 5->2102, 6->2101
                int actualSlotNo = mapSlotNumberToActualSlot(tray.slotNo);
                QString slotNoStr = QString::number(actualSlotNo);
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
                
                // 数量 - 空托盘的数量应该为0
                int displayCount = count;
                if (!tray.isRealTray && (tray.statusStr == "空托盘搬入" || tray.statusStr == "空托盘搬出")) {
                    displayCount = 0;
                }
                QTableWidgetItem* item4 = new QTableWidgetItem(QString::number(displayCount));
                item4->setTextAlignment(Qt::AlignCenter);
                ui->tableWidget->setItem(tableRow, 4, item4);
                
                // 时间
                QTableWidgetItem* item5 = new QTableWidgetItem(currentTime);
                item5->setTextAlignment(Qt::AlignCenter);
                ui->tableWidget->setItem(tableRow, 5, item5);
                
                // 保存到数据库 - 空托盘的数量应该为0
                int saveCount = count;
                if (!tray.isRealTray && (tray.statusStr == "空托盘搬入" || tray.statusStr == "空托盘搬出")) {
                    saveCount = 0;
                }
                int slotNoInt = actualSlotNo; // 使用映射后的实际滑槽号
                qDebug() << QString("processHexData: 准备保存数据 - slotNoStr=%1, slotNoInt=%2, tray.slotNo=%3, isRealTray=%4")
                            .arg(slotNoStr).arg(slotNoInt).arg(tray.slotNo).arg(tray.isRealTray);
                insertDataRecord(slotNoInt, tray.statusStr, vehicleName, vehicleCode, saveCount, currentTime);
                
                // 给当前班次表格添加记录
                addDataToCurrentShiftTable(slotNoInt, tray.statusStr, vehicleCode, vehicleName, saveCount, currentTime);
                
                appendToLog(QString("托盘%1数据已添加到表格 - %2, 车型: %3").arg(slotNoStr).arg(tray.statusStr).arg(vehicleName), false);
                
                // 如果是实托盘搬出，更新可视化界面
                // 根据字节位置确定放置位置：第5个字节->位置0，第6个字节->位置1，第7个字节->位置2
                if (tray.isRealTray && tray.statusStr == "实托盘搬出" && !vehicleName.isEmpty()) {
                    // tray.slotNo是1-3，对应第5-7个字节，转换为位置索引0-2
                    int slotIndex = tray.slotNo - 1;
                    updateVisualization(vehicleName, true, slotIndex);
                }
                // 如果是空托盘搬入，处理空托盘搬入逻辑
                // 根据字节位置确定检查位置：第8个字节（slotNo=4）->2103，对应右边槽位（位置2），第9个字节（slotNo=5）->2102，对应中间槽位（位置1），第10个字节（slotNo=6）->2101，对应左边槽位（位置0）
                if (!tray.isRealTray && tray.statusStr == "空托盘搬入" && !vehicleName.isEmpty()) {
                    handleEmptyTrayIn(vehicleName, tray.slotNo);
                }
            } else if (tray.modelCode != 0) {
                appendToLog(QString("托盘%1的车型代码 %2 未在绑定表中找到").arg(tray.slotNo).arg(modelCodeStr), true);
            }
        }
    }

    // 如果工程组页面可见且显示当前班次，更新工程组统计表格
    if (ui->stackedWidget->currentIndex() == 4 && m_projectGroupDisplayShift == "current") {
        updateProjectGroupStatistics();
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
        QTableWidgetItem* codeItem = ui->tableWidgetVehicleBinding->item(row, 1);
        QTableWidgetItem* nameItem = ui->tableWidgetVehicleBinding->item(row, 2);
        QTableWidgetItem* countItem = ui->tableWidgetVehicleBinding->item(row, 3);
        
        // 检查表格项是否存在
        if (!codeItem || !nameItem || !countItem) {
            qDebug() << "Table items not found for row:" << row;
            continue; // 跳过这一行，继续查找下一行
        }
        
        QString code = codeItem->text().trimmed(); // 车型代码列（10进制格式）
        if (modelCode1 == code) {
            vehicleCode1 = code; // 车型代码
            vehicleName1 = nameItem->text(); // 车型名称
            count1 = countItem->text().toInt();
        }
        if (modelCode2 == code) {
            vehicleCode2 = code; // 车型代码
            vehicleName2 = nameItem->text(); // 车型名称
            count2 = countItem->text().toInt();
        }
        if (modelCode3 == code) {
            vehicleCode3 = code; // 车型代码
            vehicleName3 = nameItem->text(); // 车型名称
            count3 = countItem->text().toInt();
        }
        if (modelCode4 == code) {
            vehicleCode4 = code; // 车型代码
            vehicleName4 = nameItem->text(); // 车型名称
            count4 = countItem->text().toInt();
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
        addDataToCurrentShiftTable(1, statusStr1, vehicleCode1, vehicleName1, count1, currentTime);
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
        addDataToCurrentShiftTable(2, statusStr1, vehicleCode2, vehicleName2, count2, currentTime);
    }
    // 添加第6字节（满托/空托）数据
    // 空托盘的数量应该为0
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
        // 空托盘数量设置为0
        QTableWidgetItem* t16 = new QTableWidgetItem("0");
        t16->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row1, 4, t16);
        QTableWidgetItem* t17 = new QTableWidgetItem(currentTime);
        t17->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row1, 5, t17);
        insertDataRecord(1, statusStr2, vehicleName3, vehicleCode3, 0, currentTime);
        addDataToCurrentShiftTable(1, statusStr2, vehicleCode3, vehicleName3, 0, currentTime);
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
        // 空托盘数量设置为0
        QTableWidgetItem* t22 = new QTableWidgetItem("0");
        t22->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row2, 4, t22);
        QTableWidgetItem* t23 = new QTableWidgetItem(currentTime);
        t23->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(row2, 5, t23);
        insertDataRecord(2, statusStr2, vehicleName4, vehicleCode4, 0, currentTime);
        addDataToCurrentShiftTable(2, statusStr2, vehicleCode4, vehicleName4, 0, currentTime);
    }
    // 记录到日志
    if ((status1 != 0x00 && (!vehicleName1.isEmpty() || !vehicleName2.isEmpty())) ||
        (status2 != 0x00 && (!vehicleName3.isEmpty() || !vehicleName4.isEmpty()))) {
        appendToLog(QString("数据已添加到表格 - 时间: %1").arg(currentTime), false);
        
        // 如果工程组页面可见且显示当前班次，更新工程组统计表格
        if (ui->stackedWidget->currentIndex() == 4 && m_projectGroupDisplayShift == "current") {
            updateProjectGroupStatistics();
        }
    }
}

/**
 * @brief 添加数据到当前班次表格
 * @param slotNo 滑槽号
 * @param status 状态
 * @param modelCode 车型代码
 * @param modelName 车型名称
 * @param count 数量
 * @param currentTime 时间
 */
void tcpClient::addDataToCurrentShiftTable(int slotNo, const QString &status, const QString &modelCode,
                                           const QString &modelName, int count, const QString &currentTime)
{
    // 根据状态选择对应的表格
    QTableWidget* targetTable = nullptr;
    if (status == "实托盘搬入") {
        targetTable = realTrayInTableWidget;
    } else if (status == "实托盘搬出") {
        targetTable = realTrayOutTableWidget;
    } else if (status == "空托盘搬入") {
        targetTable = emptyTrayInTableWidget;
    } else if (status == "空托盘搬出") {
        targetTable = emptyTrayOutTableWidget;
    } else {
        qWarning() << "未知状态，无法添加到表格:" << status;
        return;
    }
    
    if (!targetTable) {
        qWarning() << QString("目标表格为空，无法添加记录: status=%1").arg(status);
        return;
    }
    
    qDebug() << QString("addDataToCurrentShiftTable: slotNo=%1, status=%2, modelName=%3, currentTime=%4")
                .arg(slotNo).arg(status).arg(modelName).arg(currentTime);
    
    int tableRow = targetTable->rowCount();
    targetTable->insertRow(tableRow);
    
    // 滑槽号（列0）
    QTableWidgetItem* item0 = new QTableWidgetItem(QString::number(slotNo));
    item0->setTextAlignment(Qt::AlignCenter);
    targetTable->setItem(tableRow, 0, item0);
    
    // 车型代码（列1）
    QTableWidgetItem* item1 = new QTableWidgetItem(modelCode);
    item1->setTextAlignment(Qt::AlignCenter);
    targetTable->setItem(tableRow, 1, item1);
    
    // 车型名称（列2）
    QTableWidgetItem* item2 = new QTableWidgetItem(modelName);
    item2->setTextAlignment(Qt::AlignCenter);
    targetTable->setItem(tableRow, 2, item2);
    
    // 数量（列3）
    QTableWidgetItem* item3 = new QTableWidgetItem(QString::number(count));
    item3->setTextAlignment(Qt::AlignCenter);
    targetTable->setItem(tableRow, 3, item3);
    
    // 时间（列4）
    QTableWidgetItem* item4 = new QTableWidgetItem(currentTime);
    item4->setTextAlignment(Qt::AlignCenter);
    targetTable->setItem(tableRow, 4, item4);
    
    // 注意：数据已保存到 data_records 表，当前班次表格从 data_records 表加载，无需再次保存
    
    qDebug() << QString("已成功添加到当前班次表格，行号: %1").arg(tableRow);
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

    // 更新总成指示表
    loadVehicleModelsToAssemblyIndicator();

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

    // 更新总成指示表
    loadVehicleModelsToAssemblyIndicator();

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
    
    // 更新总成指示表
    loadVehicleModelsToAssemblyIndicator();
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
    
    // 忽略序号列（列0）的变化，因为序号列不应该触发数据库更新
    if (col == 0) {
        return;
    }
    
    QString newValue = item->text();

    // 获取该行的完整信息，添加安全检查
    // 确保所有必需的列都已创建
    QTableWidgetItem* codeItem = ui->tableWidgetVehicleBinding->item(row, 1);
    QTableWidgetItem* nameItem = ui->tableWidgetVehicleBinding->item(row, 2);
    QTableWidgetItem* countItem = ui->tableWidgetVehicleBinding->item(row, 3);

    if (!codeItem || !nameItem || !countItem) {
        // 如果某些列还没有创建，说明可能正在加载数据，忽略此次变化
        qDebug() << "Table items not found for row:" << row << ", column:" << col << ". Skipping update (likely during data loading).";
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
    
    // 更新总成指示表（当车型名称或数量变化时）
    if (col == 2 || col == 3) { // 车型名称列或数量列
        loadVehicleModelsToAssemblyIndicator();
    }
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

    // 可视化记录表
    if (query.exec("CREATE TABLE IF NOT EXISTS visualization_records ("
                   "id INT PRIMARY KEY AUTO_INCREMENT,"
                   "slot_position INT,"
                   "vehicle_name VARCHAR(255),"
                   "update_time VARCHAR(255))")) {
        qDebug() << "可视化记录表创建/检查成功";
    } else {
        qWarning() << "可视化记录表创建失败:" << query.lastError().text();
    }
    
    // 空托盘可视化记录表
    if (query.exec("CREATE TABLE IF NOT EXISTS empty_tray_visualization_records ("
                   "id INT PRIMARY KEY AUTO_INCREMENT,"
                   "slot_position INT,"
                   "vehicle_name VARCHAR(255),"
                   "update_time VARCHAR(255))")) {
        qDebug() << "空托盘可视化记录表创建/检查成功";
    } else {
        qWarning() << "空托盘可视化记录表创建失败:" << query.lastError().text();
    }

    // 连接配置表
    if (query.exec("CREATE TABLE IF NOT EXISTS connection_configs ("
                   "id INT PRIMARY KEY AUTO_INCREMENT,"
                   "config_type VARCHAR(50),"
                   "ip_address VARCHAR(50),"
                   "port INT,"
                   "update_time VARCHAR(255))")) {
        qDebug() << "连接配置表创建/检查成功";
    } else {
        qWarning() << "连接配置表创建失败:" << query.lastError().text();
    }

    // 统计信息表
    if (query.exec("CREATE TABLE IF NOT EXISTS statistics_info ("
                   "id INT PRIMARY KEY AUTO_INCREMENT,"
                   "stat_type VARCHAR(50) UNIQUE,"
                   "count_value INT,"
                   "update_time VARCHAR(255))")) {
        qDebug() << "统计信息表创建/检查成功";
    } else {
        qWarning() << "统计信息表创建失败:" << query.lastError().text();
    }
    
    // 初始化班次记录表
    initShiftTable();
    
    // 初始化班次设置表
    initShiftConfigTable();
    
    // 当前班次表格记录表
    if (query.exec("CREATE TABLE IF NOT EXISTS current_shift_records ("
                   "id INT PRIMARY KEY AUTO_INCREMENT,"
                   "slot_no INT,"
                   "status VARCHAR(255),"
                   "model_code VARCHAR(255),"
                   "model_name VARCHAR(255),"
                   "count INT,"
                   "time VARCHAR(255))")) {
        qDebug() << "当前班次表格记录表创建/检查成功";
    } else {
        qWarning() << "当前班次表格记录表创建失败:" << query.lastError().text();
    }

    // 异常记录表
    if (query.exec("CREATE TABLE IF NOT EXISTS exception_records ("
                   "id INT PRIMARY KEY AUTO_INCREMENT,"
                   "slot_no INT,"
                   "model_name VARCHAR(255),"
                   "status VARCHAR(255),"
                   "exception_info VARCHAR(500),"
                   "date VARCHAR(255))")) {
        qDebug() << "异常记录表创建/检查成功";
    } else {
        qWarning() << "异常记录表创建失败:" << query.lastError().text();
    }

    qInfo() << "数据库初始化完成";
}

/**
 * @brief 将指令滑槽号映射为实际滑槽号
 * @param slotNumber 指令中的滑槽号（1-6）
 * @return 实际滑槽号（2001, 2002, 2003, 2103, 2102, 2101），如果不在范围内则返回原值
 */
int tcpClient::mapSlotNumberToActualSlot(int slotNumber)
{
    switch (slotNumber) {
        case 1: return 2001;
        case 2: return 2002;
        case 3: return 2003;
        case 4: return 2103;
        case 5: return 2102;
        case 6: return 2101;
        default: return slotNumber; // 如果不在1-6范围内，返回原值
    }
}

/**
 * @brief 根据槽位索引获取滑槽号
 * @param slotIndex 槽位索引（0-20）
 * @param isRealTray 是否为实托盘
 * @return 滑槽号（实托盘：索引18、19、20对应2101、2102、2103；空托盘：索引18、19、20对应2003、2002、2001）
 */
int tcpClient::getSlotNoFromIndex(int slotIndex, bool isRealTray)
{
    // 对于最靠近出口的3个槽位（索引18、19、20），直接映射
    // 注意：空托盘索引18和20互换（因为出口PLC这2个位置是反过来的）
    if (slotIndex == 18) {
        return isRealTray ? 2101 : 2003;
    } else if (slotIndex == 19) {
        return isRealTray ? 2102 : 2002;
    } else if (slotIndex == 20) {
        return isRealTray ? 2103 : 2001;
    }
    
    // 对于其他槽位，根据行和列计算
    // 每行3个槽位，索引0-2是第一行，3-5是第二行，等等
    // 实托盘：列标签2101、2102、2103（第7行，索引18、19、20）
    // 空托盘：列标签2003、2002、2001（第7行，索引18、19、20）
    // 其他行的映射：行0对应1001/1101、1002/1102、1003/1103（实托盘）或2001、2002、2003（空托盘）
    // 这里简化处理，对于非18、19、20的索引，返回一个基于索引的计算值
    // 实际上，根据布局，索引i对应的列是 i % 3，行是 i / 3
    int row = slotIndex / 3;
    int col = slotIndex % 3;
    
    if (isRealTray) {
        // 实托盘：根据行和列计算滑槽号
        // 第7行（索引18、19、20）对应2101、2102、2103
        // 第0行（索引0、1、2）对应1001、1002、1003
        // 这里简化处理，只处理最靠近出口的情况
        if (row == 6) { // 第7行（索引18、19、20）
            return 2101 + col;
        } else if (row == 0) { // 第1行（索引0、1、2）
            return 1001 + col;
        } else {
            // 其他行，返回一个基于索引的计算值
            // 这里返回索引+1作为默认值，实际应该根据具体布局确定
            return slotIndex + 1;
        }
    } else {
        // 空托盘：根据行和列计算滑槽号
        // 第7行（索引18、19、20）对应2003、2002、2001（注意：索引18对应2003，索引20对应2001）
        if (row == 6) { // 第7行（索引18、19、20）
            // 注意：由于2001和2003互换，索引18对应2003，索引19对应2002，索引20对应2001
            return 2003 - col;
        } else if (row == 0) { // 第1行（索引0、1、2）对应2001、2002、2003
            return 2001 + col;
        } else {
            // 其他行，返回一个基于索引的计算值
            // 这里返回索引+1作为默认值，实际应该根据具体布局确定
            return slotIndex + 1;
        }
    }
}

void tcpClient::insertDataRecord(int slotNo, const QString &status, const QString &modelName, const QString &modelCode, int count, const QString &currentTime) {
    qDebug() << QString("insertDataRecord被调用: slotNo=%1, status=%2, modelName=%3, modelCode=%4, count=%5, time=%6")
                .arg(slotNo).arg(status).arg(modelName).arg(modelCode).arg(count).arg(currentTime);
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

/**
 * @brief 插入当前班次表格记录到数据库
 */
void tcpClient::insertCurrentShiftRecord(int slotNo, const QString &status, const QString &modelCode, const QString &modelName, int count, const QString &currentTime)
{
    QSqlQuery query;
    query.prepare("INSERT INTO current_shift_records (slot_no, status, model_code, model_name, count, time) VALUES (?, ?, ?, ?, ?, ?)");
    query.addBindValue(slotNo);
    query.addBindValue(status);
    query.addBindValue(modelCode);
    query.addBindValue(modelName);
    query.addBindValue(count);
    query.addBindValue(currentTime);
    if (!query.exec()) {
        appendToLog("插入当前班次表格记录失败: " + query.lastError().text(), true);
        qWarning() << "插入当前班次表格记录失败:" << query.lastError().text();
    }
}

/**
 * @brief 从数据库删除当前班次表格记录
 */
void tcpClient::deleteCurrentShiftRecord(int slotNo, const QString &status, const QString &modelCode, const QString &modelName, int count, const QString &currentTime)
{
    QSqlQuery query;
    query.prepare("DELETE FROM current_shift_records WHERE slot_no = ? AND status = ? AND model_code = ? AND model_name = ? AND count = ? AND time = ?");
    query.addBindValue(slotNo);
    query.addBindValue(status);
    query.addBindValue(modelCode);
    query.addBindValue(modelName);
    query.addBindValue(count);
    query.addBindValue(currentTime);
    if (!query.exec()) {
        appendToLog("删除当前班次表格记录失败: " + query.lastError().text(), true);
        qWarning() << "删除当前班次表格记录失败:" << query.lastError().text();
    }
}

/**
 * @brief 清空当前班次表格数据库记录
 */
void tcpClient::clearCurrentShiftRecords()
{
    QSqlQuery query;
    if (!query.exec("DELETE FROM current_shift_records")) {
        appendToLog("清空当前班次表格记录失败: " + query.lastError().text(), true);
        qWarning() << "清空当前班次表格记录失败:" << query.lastError().text();
    }
}

/**
 * @brief 从数据库加载当前班次表格记录
 * 从数据表格（data_records）加载数据，并根据时间过滤当前班次
 */
void tcpClient::loadCurrentShiftRecordsFromDb()
{
    // 清空所有表格
    if (realTrayInTableWidget) realTrayInTableWidget->setRowCount(0);
    if (realTrayOutTableWidget) realTrayOutTableWidget->setRowCount(0);
    if (emptyTrayInTableWidget) emptyTrayInTableWidget->setRowCount(0);
    if (emptyTrayOutTableWidget) emptyTrayOutTableWidget->setRowCount(0);
    
    // 检查表格是否已创建
    if (!realTrayInTableWidget || !realTrayOutTableWidget || 
        !emptyTrayInTableWidget || !emptyTrayOutTableWidget) {
        qWarning() << "表格未创建，无法加载记录";
        return;
    }
    
    // 如果班次设置未加载，先加载
    if (!m_shiftConfigLoaded) {
        loadShiftConfig();
    }
    
    // 获取当前班次
    QString currentShift = getCurrentShift();
    QDateTime now = QDateTime::currentDateTime();
    QDate today = now.date();
    
    // 计算当前班次的时间范围（使用数据库中的班次设置）
    QDateTime shiftStartTime, shiftEndTime;
    if (currentShift == "白班") {
        // 白班：当天 dayShiftStart - dayShiftEnd
        shiftStartTime = QDateTime(today, m_dayShiftStart);
        shiftEndTime = QDateTime(today, m_dayShiftEnd);
    } else {
        // 夜班：当天 nightShiftStart - 次日 nightShiftEnd（需要加载当天和隔天的数据）
        shiftStartTime = QDateTime(today, m_nightShiftStart);
        shiftEndTime = QDateTime(today.addDays(1), m_nightShiftEnd);
    }
    
    // 从数据表格（data_records）加载数据
    QSqlQuery query;
    query.prepare("SELECT slot_no, status, model_name, model_code, count, time FROM data_records ORDER BY time DESC");
    
    if (!query.exec()) {
        appendToLog(QString("查询数据表格记录失败: %1").arg(query.lastError().text()), true);
        qWarning() << "查询数据表格记录失败:" << query.lastError().text();
        return;
    }
    
    int loadedCount = 0;
    while (query.next()) {
        int slotNo = query.value(0).toInt();
        QString status = query.value(1).toString();
        QString modelName = query.value(2).toString();
        QString modelCode = query.value(3).toString();
        int count = query.value(4).toInt();
        QString timeStr = query.value(5).toString();
        
        // 解析记录时间
        QDateTime recordDateTime = QDateTime::fromString(timeStr, "yyyy-MM-dd HH:mm:ss");
        if (!recordDateTime.isValid()) {
            // 如果日期格式不正确，跳过该记录
            continue;
        }
        
        // 判断记录是否在当前班次的时间范围内
        bool isInShift = false;
        if (currentShift == "白班") {
            // 白班：当天 7:15 - 17:30
            isInShift = (recordDateTime.date() == today && 
                        recordDateTime >= shiftStartTime && 
                        recordDateTime < shiftEndTime);
        } else {
            // 夜班：当天 17:30 - 次日 7:15（包括当天和隔天的数据）
            isInShift = (recordDateTime >= shiftStartTime && recordDateTime < shiftEndTime);
        }
        
        // 只加载当前班次的记录
        if (!isInShift) {
            continue;
        }
        
        // 根据状态选择对应的表格
        QTableWidget* targetTable = nullptr;
        if (status == "实托盘搬入") {
            targetTable = realTrayInTableWidget;
        } else if (status == "实托盘搬出") {
            targetTable = realTrayOutTableWidget;
        } else if (status == "空托盘搬入") {
            targetTable = emptyTrayInTableWidget;
        } else if (status == "空托盘搬出") {
            targetTable = emptyTrayOutTableWidget;
        } else {
            qWarning() << "未知状态，跳过记录:" << status;
            continue;
        }
        
        if (!targetTable) {
            continue;
        }
        
        int row = targetTable->rowCount();
        targetTable->insertRow(row);
        
        // 滑槽号（列0）
        QTableWidgetItem* item0 = new QTableWidgetItem(QString::number(slotNo));
        item0->setTextAlignment(Qt::AlignCenter);
        targetTable->setItem(row, 0, item0);
        
        // 车型代码（列1）
        QTableWidgetItem* item1 = new QTableWidgetItem(modelCode);
        item1->setTextAlignment(Qt::AlignCenter);
        targetTable->setItem(row, 1, item1);
        
        // 车型名称（列2）
        QTableWidgetItem* item2 = new QTableWidgetItem(modelName);
        item2->setTextAlignment(Qt::AlignCenter);
        targetTable->setItem(row, 2, item2);
        
        // 数量（列3）
        QTableWidgetItem* item3 = new QTableWidgetItem(QString::number(count));
        item3->setTextAlignment(Qt::AlignCenter);
        targetTable->setItem(row, 3, item3);
        
        // 时间（列4）
        QTableWidgetItem* item4 = new QTableWidgetItem(timeStr);
        item4->setTextAlignment(Qt::AlignCenter);
        targetTable->setItem(row, 4, item4);
        
        loadedCount++;
    }
    
    qDebug() << QString("当前班次表格已从数据表格加载（当前班次：%1），共%2条记录").arg(currentShift).arg(loadedCount);
}

void tcpClient::loadModelBindingsFromDb() {
    // 暂时阻止信号，避免在加载数据时触发itemChanged信号
    bool wasBlocked = ui->tableWidgetVehicleBinding->blockSignals(true);
    
    try {
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
    } catch (...) {
        // 确保即使出现异常也恢复信号
        ui->tableWidgetVehicleBinding->blockSignals(wasBlocked);
        throw;
    }
    
    // 恢复信号（恢复到之前的状态）
    ui->tableWidgetVehicleBinding->blockSignals(wasBlocked);
    
    // 加载车型到总成指示表（延迟加载，确保总成指示表已创建）
    QTimer::singleShot(100, this, &tcpClient::loadVehicleModelsToAssemblyIndicator);
}

/**
 * @brief 加载绑定车型到总成指示表
 */
void tcpClient::loadVehicleModelsToAssemblyIndicator()
{
    if (!assemblyIndicatorTable) {
        qDebug() << "总成指示表未初始化，跳过加载";
        return;
    }
    
    // 清空现有数据
    assemblyIndicatorTable->setRowCount(0);
    
    // 从车型绑定表格读取数据
    for (int row = 0; row < ui->tableWidgetVehicleBinding->rowCount(); ++row) {
        QTableWidgetItem* nameItem = ui->tableWidgetVehicleBinding->item(row, 2); // 车型名称
        QTableWidgetItem* countItem = ui->tableWidgetVehicleBinding->item(row, 3); // 数量（收容数）
        
        if (!nameItem || !countItem) {
            continue;
        }
        
        QString vehicleName = nameItem->text().trimmed();
        QString capacityStr = countItem->text().trimmed();
        
        if (vehicleName.isEmpty()) {
            continue;
        }
        
        // 插入2行记录（从托盘搬运列开始需要分为2行：计划行和实际行）
        int planRow = assemblyIndicatorTable->rowCount();
        int actualRow = planRow + 1;
        assemblyIndicatorTable->insertRow(planRow);
        assemblyIndicatorTable->insertRow(actualRow);
        
        // 设置行高
        assemblyIndicatorTable->setRowHeight(planRow, 30);
        assemblyIndicatorTable->setRowHeight(actualRow, 30);
        
        // 设置各列数据
        // 前5列（列0-4）：合并为单行，不分为2行
        // 列0: 车型名称
        QTableWidgetItem* item0 = new QTableWidgetItem(vehicleName);
        item0->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
        item0->setFlags(item0->flags() & ~Qt::ItemIsEditable); // 设置为不可编辑
        assemblyIndicatorTable->setItem(planRow, 0, item0);
        assemblyIndicatorTable->setSpan(planRow, 0, 2, 1); // 合并2行1列
        
        // 列1: 收容数
        QTableWidgetItem* item1 = new QTableWidgetItem(capacityStr);
        item1->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
        item1->setFlags(item1->flags() & ~Qt::ItemIsEditable); // 设置为不可编辑
        assemblyIndicatorTable->setItem(planRow, 1, item1);
        assemblyIndicatorTable->setSpan(planRow, 1, 2, 1); // 合并2行1列
        
        // 列2: 产量（初始为空）
        QTableWidgetItem* item2 = new QTableWidgetItem("");
        item2->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
        item2->setFlags(item2->flags() & ~Qt::ItemIsEditable); // 设置为不可编辑
        assemblyIndicatorTable->setItem(planRow, 2, item2);
        assemblyIndicatorTable->setSpan(planRow, 2, 2, 1); // 合并2行1列
        
        // 列3: 生产总托数（初始为空）
        QTableWidgetItem* item3 = new QTableWidgetItem("");
        item3->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
        item3->setFlags(item3->flags() & ~Qt::ItemIsEditable); // 设置为不可编辑
        assemblyIndicatorTable->setItem(planRow, 3, item3);
        assemblyIndicatorTable->setSpan(planRow, 3, 2, 1); // 合并2行1列
        
        // 列4: 节拍（初始为空，可编辑）
        QTableWidgetItem* item4 = new QTableWidgetItem("");
        item4->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
        item4->setFlags(item4->flags() | Qt::ItemIsEditable); // 设置为可编辑
        assemblyIndicatorTable->setItem(planRow, 4, item4);
        assemblyIndicatorTable->setSpan(planRow, 4, 2, 1); // 合并2行1列
        
        // 列5: 托盘搬运 - 第一行显示"计划"，第二行显示"实际"
        QTableWidgetItem* item5_plan = new QTableWidgetItem("计划");
        item5_plan->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
        item5_plan->setFlags(item5_plan->flags() & ~Qt::ItemIsEditable); // 设置为不可编辑
        assemblyIndicatorTable->setItem(planRow, 5, item5_plan);
        QTableWidgetItem* item5_actual = new QTableWidgetItem("实际");
        item5_actual->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
        item5_actual->setFlags(item5_actual->flags() & ~Qt::ItemIsEditable); // 设置为不可编辑
        assemblyIndicatorTable->setItem(actualRow, 5, item5_actual);
        
        // 列6-38: 33列时间（8个时间列每个细分为4列：15、30、45、60 + 1个休息列1列）
        // 第一行显示计划数据，第二行显示实际数据（初始为空）
        for (int col = 6; col <= 38; ++col) {
            // 休息列（列22）跳过，最后统一处理
            if (col == 22) {
                continue;
            }
            
            QTableWidgetItem* timeItem_plan = new QTableWidgetItem("");
            timeItem_plan->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
            timeItem_plan->setFlags(timeItem_plan->flags() & ~Qt::ItemIsEditable); // 设置为不可编辑
            assemblyIndicatorTable->setItem(planRow, col, timeItem_plan);
            
            QTableWidgetItem* timeItem_actual = new QTableWidgetItem("");
            timeItem_actual->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
            timeItem_actual->setFlags(timeItem_actual->flags() & ~Qt::ItemIsEditable); // 设置为不可编辑
            assemblyIndicatorTable->setItem(actualRow, col, timeItem_actual);
        }
    }
    
    // 休息列（列22）统一处理：合并所有行，显示一个"45分钟"
    int totalRows = assemblyIndicatorTable->rowCount();
    if (totalRows > 0) {
        QTableWidgetItem* restItem = new QTableWidgetItem("45分钟");
        restItem->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
        restItem->setFlags(restItem->flags() & ~Qt::ItemIsEditable); // 设置为不可编辑
        assemblyIndicatorTable->setItem(0, 22, restItem);
        assemblyIndicatorTable->setSpan(0, 22, totalRows, 1); // 合并所有行（从第0行开始，合并totalRows行）
    }
    
    qDebug() << "已加载" << assemblyIndicatorTable->rowCount() << "个车型到总成指示表";
}

void tcpClient::loadDataRecordsFromDb() {
    // 清除筛选条件，加载所有数据
    m_tableFilters.clear();
    applyTableFilter();
}

/**
 * @brief 表格表头点击处理（筛选功能）
 * @param logicalIndex 列索引
 */
void tcpClient::onTableHeaderClicked(int logicalIndex)
{
    QString columnName;
    QString currentFilter = m_tableFilters.value(logicalIndex, "");
    
    // 根据列索引确定列名
    switch (logicalIndex) {
    case 0:
        columnName = "滑槽号";
        break;
    case 1:
        columnName = "送入送出状态";
        break;
    case 2:
        columnName = "车型代码";
        break;
    case 3:
        columnName = "车型名称";
        break;
    case 4:
        columnName = "数量（件）";
        break;
    case 5:
        columnName = "时间";
        break;
    default:
        return;
    }
    
    // 如果是时间列，使用日期选择
    if (logicalIndex == 5) {
        QDate currentDate = QDate::currentDate();
        if (!currentFilter.isEmpty()) {
            // 尝试解析已有的筛选日期
            QDate filterDate = QDate::fromString(currentFilter, "yyyy-MM-dd");
            if (filterDate.isValid()) {
                currentDate = filterDate;
            }
        }
        
        // 使用输入对话框选择日期
        bool ok;
        QString dateStr = QInputDialog::getText(this, QString("筛选%1").arg(columnName),
                                                QString("请输入日期（格式：yyyy-MM-dd，留空清除筛选）：\n例如：2025-12-31"),
                                                QLineEdit::Normal, 
                                                currentFilter.isEmpty() ? currentDate.toString("yyyy-MM-dd") : currentFilter,
                                                &ok);
        if (ok) {
            if (dateStr.isEmpty()) {
                // 清除筛选
                m_tableFilters.remove(logicalIndex);
                appendToLog(QString("已清除%1筛选").arg(columnName), false);
            } else {
                // 验证日期格式
                QDate selectedDate = QDate::fromString(dateStr, "yyyy-MM-dd");
                if (selectedDate.isValid()) {
                    m_tableFilters[logicalIndex] = selectedDate.toString("yyyy-MM-dd");
                    appendToLog(QString("已设置%1筛选：%2").arg(columnName).arg(selectedDate.toString("yyyy-MM-dd")), false);
                } else {
                    QMessageBox::warning(this, "日期格式错误", "请输入正确的日期格式：yyyy-MM-dd\n例如：2025-12-31");
                    return;
                }
            }
            applyTableFilter();
        }
    } else {
        // 其他列使用文本输入或下拉选择
        // 先获取该列的所有唯一值
        QSet<QString> uniqueValues;
        for (int row = 0; row < ui->tableWidget->rowCount(); ++row) {
            QTableWidgetItem* item = ui->tableWidget->item(row, logicalIndex);
            if (item && !item->text().isEmpty()) {
                uniqueValues.insert(item->text());
            }
        }
        
        QStringList valueList = uniqueValues.values();
        valueList.sort();
        valueList.prepend(""); // 添加空选项表示清除筛选
        
        bool ok;
        QString selectedValue;
        if (valueList.size() > 1 && valueList.size() <= 20) {
            // 如果唯一值数量较少，使用下拉选择
            selectedValue = QInputDialog::getItem(this, QString("筛选%1").arg(columnName),
                                                  QString("请选择%1（选择空值清除筛选）：").arg(columnName),
                                                  valueList, 0, false, &ok);
        } else {
            // 如果唯一值数量较多，使用文本输入
            selectedValue = QInputDialog::getText(this, QString("筛选%1").arg(columnName),
                                                  QString("请输入%1（留空清除筛选）：").arg(columnName),
                                                  QLineEdit::Normal, currentFilter, &ok);
        }
        
        if (ok) {
            if (selectedValue.isEmpty()) {
                m_tableFilters.remove(logicalIndex);
                appendToLog(QString("已清除%1筛选").arg(columnName), false);
            } else {
                m_tableFilters[logicalIndex] = selectedValue;
                appendToLog(QString("已设置%1筛选：%2").arg(columnName).arg(selectedValue), false);
            }
            applyTableFilter();
        }
    }
}

/**
 * @brief 应用表格筛选
 */
void tcpClient::applyTableFilter()
{
    if (m_tableFilters.isEmpty()) {
        // 如果没有筛选条件，重新加载所有数据
        ui->tableWidget->setRowCount(0);
        QSqlQuery query("SELECT slot_no, status, model_code, model_name, count, time FROM data_records");
        int row = 0;
        while (query.next()) {
            ui->tableWidget->insertRow(row);
            QTableWidgetItem* t0 = new QTableWidgetItem(QString::number(query.value(0).toInt())); // 直接使用数据库中的slotNo值
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
        updateTableHeaderFilterIndicator();
        return;
    }
    
    // 重新从数据库加载所有数据并应用筛选
    ui->tableWidget->setRowCount(0);
    QSqlQuery query("SELECT slot_no, status, model_code, model_name, count, time FROM data_records");
    int row = 0;
    while (query.next()) {
        QString slotNo = QString::number(query.value(0).toInt()); // 直接使用数据库中的slotNo值
        QString status = query.value(1).toString();
        QString modelCode = query.value(2).toString();
        QString modelName = query.value(3).toString();
        QString count = query.value(4).toString();
        QString time = query.value(5).toString();
        
        // 检查是否满足筛选条件
        bool match = true;
        
        // 检查滑槽号筛选
        if (m_tableFilters.contains(0)) {
            if (slotNo != m_tableFilters[0]) {
                match = false;
            }
        }
        
        // 检查状态筛选
        if (match && m_tableFilters.contains(1)) {
            if (status != m_tableFilters[1]) {
                match = false;
            }
        }
        
        // 检查车型代码筛选
        if (match && m_tableFilters.contains(2)) {
            if (modelCode != m_tableFilters[2]) {
                match = false;
            }
        }
        
        // 检查车型名称筛选
        if (match && m_tableFilters.contains(3)) {
            if (modelName != m_tableFilters[3]) {
                match = false;
            }
        }
        
        // 检查数量筛选
        if (match && m_tableFilters.contains(4)) {
            if (count != m_tableFilters[4]) {
                match = false;
            }
        }
        
        // 检查时间筛选（按天数）
        if (match && m_tableFilters.contains(5)) {
            QString filterDate = m_tableFilters[5]; // 格式：yyyy-MM-dd
            // 提取时间字符串中的日期部分
            QString timeDate = time.split(" ").first(); // 假设时间格式为 "yyyy-MM-dd hh:mm:ss"
            if (timeDate != filterDate) {
                match = false;
            }
        }
        
        // 如果满足所有筛选条件，添加到表格
        if (match) {
            ui->tableWidget->insertRow(row);
            QTableWidgetItem* t0 = new QTableWidgetItem(slotNo);
            t0->setTextAlignment(Qt::AlignCenter);
            ui->tableWidget->setItem(row, 0, t0);
            QTableWidgetItem* t1 = new QTableWidgetItem(status);
            t1->setTextAlignment(Qt::AlignCenter);
            ui->tableWidget->setItem(row, 1, t1);
            QTableWidgetItem* t2 = new QTableWidgetItem(modelCode);
            t2->setTextAlignment(Qt::AlignCenter);
            ui->tableWidget->setItem(row, 2, t2);
            QTableWidgetItem* t3 = new QTableWidgetItem(modelName);
            t3->setTextAlignment(Qt::AlignCenter);
            ui->tableWidget->setItem(row, 3, t3);
            QTableWidgetItem* t4 = new QTableWidgetItem(count);
            t4->setTextAlignment(Qt::AlignCenter);
            ui->tableWidget->setItem(row, 4, t4);
            QTableWidgetItem* t5 = new QTableWidgetItem(time);
            t5->setTextAlignment(Qt::AlignCenter);
            ui->tableWidget->setItem(row, 5, t5);
            ++row;
        }
    }
    
    // 更新表头显示筛选状态
    updateTableHeaderFilterIndicator();
}

/**
 * @brief 更新表头筛选指示器
 */
void tcpClient::updateTableHeaderFilterIndicator()
{
    QStringList originalHeaders;
    originalHeaders << "滑槽号" << "送入送出状态" << "车型代码" << "车型名称" << "数量（件）" << "时间";
    
    for (int i = 0; i < ui->tableWidget->columnCount() && i < originalHeaders.size(); ++i) {
        QString headerText = originalHeaders[i];
        
        if (m_tableFilters.contains(i)) {
            // 如果有筛选条件，在表头添加标记
            headerText += " [筛选]";
        }
        
        // 设置表头文本
        if (ui->tableWidget->horizontalHeaderItem(i)) {
            ui->tableWidget->horizontalHeaderItem(i)->setText(headerText);
        } else {
            ui->tableWidget->setHorizontalHeaderItem(i, new QTableWidgetItem(headerText));
        }
    }
}

/**
 * @brief 清除表格筛选
 */
void tcpClient::clearTableFilter()
{
    m_tableFilters.clear();
    applyTableFilter();
    appendToLog("已清除所有筛选条件", false);
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

/**
 * @brief 事件过滤器，用于处理标签双击事件
 */
bool tcpClient::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonDblClick) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            if (obj == plannedCountLabel) {
                onPlannedCountLabelDoubleClicked();
                return true;
            } else if (obj == actualCountLabel) {
                onActualCountLabelDoubleClicked();
                return true;
            } else if (obj == delayedCountLabel) {
                onDelayedCountLabelDoubleClicked();
                return true;
            }
            
            // 检查是否是实滑槽标签（索引1-21，跳过入口0和出口22）
            for (int i = 1; i <= 21 && i < m_realTrayLabels.size(); ++i) {
                if (obj == m_realTrayLabels[i]) {
                    onTraySlotLabelDoubleClicked(m_realTrayLabels[i], true, i - 1); // slotIndex是数组索引0-20
                    return true;
                }
            }
            
            // 检查是否是空滑槽标签（索引1-21，跳过入口0和出口22）
            for (int i = 1; i <= 21 && i < m_emptyTrayLabels.size(); ++i) {
                if (obj == m_emptyTrayLabels[i]) {
                    onTraySlotLabelDoubleClicked(m_emptyTrayLabels[i], false, i - 1); // slotIndex是数组索引0-20
                    return true;
                }
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

/**
 * @brief 计划便次标签双击处理
 */
void tcpClient::onPlannedCountLabelDoubleClicked()
{
    // 验证密码
    if (!showPasswordDialog("密码验证", "请输入密码以编辑计划便次:")) {
        return;
    }
    
    // 弹出数字输入对话框
    bool ok;
    int newValue = QInputDialog::getInt(this, "编辑计划便次", 
                                        "请输入计划便次:", 
                                        m_plannedCount, 0, 999999, 1, &ok);
    if (ok) {
        m_plannedCount = newValue;
        plannedCountLabel->setText(QString("计划便次：%1便").arg(m_plannedCount));
        appendToLog(QString("计划便次已更新为：%1便").arg(m_plannedCount), false);
        saveStatisticsInfo(); // 保存到数据库
    }
}

/**
 * @brief 实际便次标签双击处理
 */
void tcpClient::onActualCountLabelDoubleClicked()
{
    // 验证密码
    if (!showPasswordDialog("密码验证", "请输入密码以编辑实际便次:")) {
        return;
    }
    
    // 弹出数字输入对话框
    bool ok;
    int newValue = QInputDialog::getInt(this, "编辑实际便次", 
                                        "请输入实际便次:", 
                                        m_actualCount, 0, 999999, 1, &ok);
    if (ok) {
        m_actualCount = newValue;
        if (m_currentDisplayShift == "current" && actualCountLabel) {
            actualCountLabel->setText(QString("实际便次：%1便").arg(m_actualCount));
        }
        m_displayedActualCount = m_actualCount; // 更新显示值
        appendToLog(QString("实际便次已更新为：%1便").arg(m_actualCount), false);
        saveStatisticsInfo(); // 保存到数据库
    }
}

/**
 * @brief 延迟便次标签双击处理
 */
void tcpClient::onDelayedCountLabelDoubleClicked()
{
    // 验证密码
    if (!showPasswordDialog("密码验证", "请输入密码以编辑延迟便次:")) {
        return;
    }
    
    // 弹出数字输入对话框
    bool ok;
    int newValue = QInputDialog::getInt(this, "编辑延迟便次", 
                                        "请输入延迟便次:", 
                                        m_delayedCount, 0, 999999, 1, &ok);
    if (ok) {
        m_delayedCount = newValue;
        delayedCountLabel->setText(QString("延迟便次：%1便").arg(m_delayedCount));
        appendToLog(QString("延迟便次已更新为：%1便").arg(m_delayedCount), false);
        saveStatisticsInfo(); // 保存到数据库
    }
}

/**
 * @brief 获取所有绑定的车型名称列表
 */
QStringList tcpClient::getVehicleModelList()
{
    QStringList modelList;
    
    // 检查数据库是否已打开
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "数据库未打开，无法获取车型列表";
        return modelList;
    }
    
    QSqlQuery query("SELECT DISTINCT model_name FROM model_bindings ORDER BY model_name");
    
    if (query.lastError().isValid()) {
        qDebug() << "查询车型列表失败:" << query.lastError().text();
        return modelList;
    }
    
    while (query.next()) {
        QString modelName = query.value(0).toString().trimmed();
        if (!modelName.isEmpty()) {
            modelList.append(modelName);
        }
    }
    
    return modelList;
}

/**
 * @brief 滑槽标签双击处理
 * @param label 被双击的标签
 * @param isRealTray 是否为实滑槽
 * @param slotIndex 槽位索引（0-20）
 */
void tcpClient::onTraySlotLabelDoubleClicked(QLabel* label, bool isRealTray, int slotIndex)
{
    if (!label || slotIndex < 0 || slotIndex >= 21) {
        return;
    }
    
    // 获取当前显示的车型名称
    QString currentModel = label->text().trimmed();
    
    // 获取所有绑定的车型列表
    QStringList modelList = getVehicleModelList();
    
    // 创建对话框
    QDialog dialog(this);
    dialog.setWindowTitle(isRealTray ? QString("编辑实滑槽槽位%1").arg(slotIndex + 1) : QString("编辑空滑槽槽位%1").arg(slotIndex + 1));
    dialog.setMinimumWidth(300);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    // 添加说明标签
    QLabel* infoLabel = new QLabel("请选择车型或清空:", &dialog);
    layout->addWidget(infoLabel);
    
    // 创建下拉框
    QComboBox* comboBox = new QComboBox(&dialog);
    comboBox->setEditable(false);
    
    // 添加"清空"选项
    comboBox->addItem("（清空）", "");
    
    // 添加所有车型
    for (const QString& model : modelList) {
        comboBox->addItem(model, model);
    }
    
    // 设置当前选中的车型
    int currentIndex = 0; // 默认选中"清空"
    if (!currentModel.isEmpty()) {
        int foundIndex = comboBox->findData(currentModel);
        if (foundIndex >= 0) {
            currentIndex = foundIndex;
        } else {
            // 如果当前车型不在列表中，也添加到下拉框
            comboBox->addItem(currentModel, currentModel);
            currentIndex = comboBox->count() - 1;
        }
    }
    comboBox->setCurrentIndex(currentIndex);
    
    layout->addWidget(comboBox);
    
    // 添加按钮
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);
    
    // 显示对话框
    if (dialog.exec() == QDialog::Accepted) {
        QString selectedModel = comboBox->currentData().toString();
        
        // 更新标签显示
        label->setText(selectedModel);
        
        // 更新数组
        if (isRealTray) {
            if (slotIndex < m_realTraySlots.size()) {
                m_realTraySlots[slotIndex] = selectedModel;
            }
            // 保存到数据库
            saveVisualizationRecords();
            appendToLog(QString("实滑槽槽位%1已更新为：%2").arg(slotIndex + 1).arg(selectedModel.isEmpty() ? "（清空）" : selectedModel), false);
        } else {
            if (slotIndex < m_emptyTraySlots.size()) {
                m_emptyTraySlots[slotIndex] = selectedModel;
            }
            // 保存到数据库
            saveEmptyTrayVisualizationRecords();
            appendToLog(QString("空滑槽槽位%1已更新为：%2").arg(slotIndex + 1).arg(selectedModel.isEmpty() ? "（清空）" : selectedModel), false);
        }
    }
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

/**
 * @brief 更新可视化界面
 * @param vehicleName 车型名称
 * @param isRealTray 是否为实托盘
 */
void tcpClient::updateVisualization(const QString &vehicleName, bool isRealTray, int slotIndex)
{
    if (!isRealTray) {
        return; // 目前只处理实托盘
    }
    
    // 如果指定了位置（slotIndex >= 0），直接使用该位置
    if (slotIndex >= 0 && slotIndex <= 2) {
        // 如果是第一次（批次计数为0），先推进3个位置
        if (m_realTrayBatchCount == 0) {
            advanceVisualizationBy3();
        }
        
        // 根据位置索引写入对应的槽位
        if (m_realTraySlots.size() > slotIndex) {
            m_realTraySlots[slotIndex] = vehicleName;
            
            // 更新对应槽的标签显示（标签索引slotIndex+1，跳过入口标签0）
            if (m_realTrayLabels.size() > (slotIndex + 1) && m_realTrayLabels[slotIndex + 1]) {
                m_realTrayLabels[slotIndex + 1]->setText(vehicleName);
            }
            
            // 更新批次计数
            m_realTrayBatchCount = (m_realTrayBatchCount + 1) % 3;
            
            // 保存到数据库
            saveVisualizationRecords();
            
            appendToLog(QString("可视化界面：实滑槽第%1个槽添加车型 %2（来自第%3个字节）")
                       .arg(slotIndex + 1)
                       .arg(vehicleName)
                       .arg(5 + slotIndex), false);
        } else {
            appendToLog(QString("可视化界面：无法添加车型到第%1个槽: %2").arg(slotIndex + 1).arg(vehicleName), true);
        }
        return;
    }
    
    // 如果没有指定位置，使用原来的批次计数逻辑
    // 判断是否是第一次搬入（批次计数为0）
    if (m_realTrayBatchCount == 0) {
        // 第一次搬入：推进3个位置，然后写入位置0
        advanceVisualizationBy3();
        
        // 写入位置0（第一个槽）
        if (m_realTraySlots.size() > 0) {
            m_realTraySlots[0] = vehicleName;
            
            // 更新第一个槽的标签显示（标签索引1，跳过入口标签0）
            if (m_realTrayLabels.size() > 1 && m_realTrayLabels[1]) {
                m_realTrayLabels[1]->setText(vehicleName);
            }
            
            // 批次计数+1
            m_realTrayBatchCount = 1;
            
            // 保存到数据库
            saveVisualizationRecords();
            
            appendToLog(QString("可视化界面：实滑槽第一个槽添加车型 %1（批次第1个）").arg(vehicleName), false);
        } else {
            appendToLog(QString("可视化界面：所有槽位已满，无法添加车型: %1").arg(vehicleName), true);
        }
    } else if (m_realTrayBatchCount == 1) {
        // 第二次搬入：不移动，直接写入位置1
        if (m_realTraySlots.size() > 1) {
            m_realTraySlots[1] = vehicleName;
            
            // 更新第二个槽的标签显示（标签索引2）
            if (m_realTrayLabels.size() > 2 && m_realTrayLabels[2]) {
                m_realTrayLabels[2]->setText(vehicleName);
            }
            
            // 批次计数+1
            m_realTrayBatchCount = 2;
            
            // 保存到数据库
            saveVisualizationRecords();
            
            appendToLog(QString("可视化界面：实滑槽第二个槽添加车型 %1（批次第2个）").arg(vehicleName), false);
        } else {
            appendToLog(QString("可视化界面：无法添加车型到第二个槽: %1").arg(vehicleName), true);
        }
    } else if (m_realTrayBatchCount == 2) {
        // 第三次搬入：不移动，直接写入位置2，然后重置批次计数
        if (m_realTraySlots.size() > 2) {
            m_realTraySlots[2] = vehicleName;
            
            // 更新第三个槽的标签显示（标签索引3）
            if (m_realTrayLabels.size() > 3 && m_realTrayLabels[3]) {
                m_realTrayLabels[3]->setText(vehicleName);
            }
            
            // 批次计数重置为0，准备下一批次
            m_realTrayBatchCount = 0;
            
            // 保存到数据库
            saveVisualizationRecords();
            
            appendToLog(QString("可视化界面：实滑槽第三个槽添加车型 %1（批次第3个，批次完成）").arg(vehicleName), false);
        } else {
            appendToLog(QString("可视化界面：无法添加车型到第三个槽: %1").arg(vehicleName), true);
        }
    }
}

/**
 * @brief 推进可视化显示3个位置
 */
void tcpClient::advanceVisualizationBy3()
{
    // 从右向左推进3个位置，避免覆盖
    // 数组索引0-20对应21个槽位
    
    // 先检查21个槽位是否都满了
    bool allSlotsFull = true;
    for (int i = 0; i < 21 && i < m_realTraySlots.size(); ++i) {
        if (m_realTraySlots[i].isEmpty()) {
            allSlotsFull = false;
            break;
        }
    }
    
    // 先保存当前状态到临时数组，避免覆盖
    QVector<QString> tempSlots = m_realTraySlots;
    
    // 如果21个槽位都满了，先清空最外面3个槽位（索引18、19、20），不写入到出口标签
    if (allSlotsFull) {
        // 记录异常信息：最外面3个槽位（索引18、19、20）对应的列标签是2101、2102、2103
        QStringList columnLabels = {"2101", "2102", "2103"};
        QString currentDate = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
        
        // 记录索引18的异常（列标签2101）
        if (tempSlots.size() > 18 && !tempSlots[18].isEmpty()) {
            insertExceptionRecord(2101, tempSlots[18], "实托盘搬入", "槽位满，异常取出", currentDate);
        }
        
        // 记录索引19的异常（列标签2102）
        if (tempSlots.size() > 19 && !tempSlots[19].isEmpty()) {
            insertExceptionRecord(2102, tempSlots[19], "实托盘搬入", "槽位满，异常取出", currentDate);
        }
        
        // 记录索引20的异常（列标签2103）
        if (tempSlots.size() > 20 && !tempSlots[20].isEmpty()) {
            insertExceptionRecord(2103, tempSlots[20], "实托盘搬入", "槽位满，异常取出", currentDate);
        }
        
        // 清空最外面3个槽位（索引18、19、20），不写入到出口标签
        m_realTraySlots[18] = "";
        m_realTraySlots[19] = "";
        m_realTraySlots[20] = "";
        
        // 确保出口标签保持初始文本，不写入内容
        if (m_realTrayLabels.size() > 22 && m_realTrayLabels[22]) {
            m_realTrayLabels[22]->setText(m_realTrayExitLabelText);
        }
        
        appendToLog("21个槽位已满，已清空最外面3个槽位（索引18、19、20），开始推进所有槽位，已记录异常信息", false);
    } else {
        // 如果没满，使用原来的逻辑处理最后3个槽位
        // 如果索引20有内容，直接清空，不写入到出口标签
        if (tempSlots.size() > 20 && !tempSlots[20].isEmpty()) {
            // 不写入到出口标签，直接清空
            m_realTraySlots[20] = "";
        }
        
        // 如果索引19有内容，移动到索引20
        if (tempSlots.size() > 19 && !tempSlots[19].isEmpty()) {
            if (m_realTraySlots.size() > 20) {
                m_realTraySlots[20] = tempSlots[19];
                m_realTraySlots[19] = "";
            }
        }
        
        // 如果索引18有内容，移动到索引19
        if (tempSlots.size() > 18 && !tempSlots[18].isEmpty()) {
            if (m_realTraySlots.size() > 19) {
                m_realTraySlots[19] = tempSlots[18];
                m_realTraySlots[18] = "";
            }
        }
    }
    
    // 从右向左推进3个位置（从索引17到索引0）
    // 这样所有槽位都会向下移动3个位置，为新的数据腾出空间
    for (int i = 17; i >= 0; --i) {
        if (i < tempSlots.size() && !tempSlots[i].isEmpty()) {
            // 向右移动3格
            int targetIndex = i + 3;
            if (targetIndex < m_realTraySlots.size()) {
                m_realTraySlots[targetIndex] = tempSlots[i];
                m_realTraySlots[i] = "";
            }
        }
    }
    
    // 更新所有滑槽标签显示（标签索引1-21对应数组索引0-20）
    for (int i = 0; i < 21 && i < m_realTraySlots.size(); ++i) {
        int labelIndex = i + 1; // 标签索引 = 数组索引 + 1（跳过入口标签0）
        if (labelIndex < m_realTrayLabels.size() && m_realTrayLabels[labelIndex]) {
            m_realTrayLabels[labelIndex]->setText(m_realTraySlots[i]);
        }
    }
    
    // 清除出口标签（如果索引20为空，确保出口标签保持初始文本）
    if (m_realTrayLabels.size() > 22 && m_realTrayLabels[22]) {
        if (m_realTraySlots.size() <= 20 || m_realTraySlots[20].isEmpty()) {
            // 索引20为空，确保出口标签保持初始文本
            m_realTrayLabels[22]->setText(m_realTrayExitLabelText);
        } else {
            // 如果满槽清空后，索引20可能被移动过来的数据填充，但不应该写入到出口标签
            // 只有在非满槽情况下，索引20的内容才会写入到出口标签（已在上面处理）
        }
    }
    
    // 保存可视化记录到数据库
    saveVisualizationRecords();
}

/**
 * @brief 推进可视化显示
 */
void tcpClient::advanceVisualization()
{
    // 从右向左推进，避免覆盖
    // 数组索引0是入口，索引1-19是滑槽位置，索引20是出口
    // 从最后一个槽（索引19）开始，依次向右移动到出口（索引20）
    
    // 如果最后一个槽（索引20）有内容，直接清空，不写入到出口标签
    if (m_realTraySlots.size() > 20 && !m_realTraySlots[20].isEmpty()) {
        // 不写入到出口标签，直接清空
        m_realTraySlots[20] = "";
    }
    
    // 从右向左推进（从索引19到索引0）
    for (int i = 19; i >= 0; --i) {
        if (i < m_realTraySlots.size() && !m_realTraySlots[i].isEmpty()) {
            // 向右移动一格
            if (i + 1 < m_realTraySlots.size()) {
                m_realTraySlots[i + 1] = m_realTraySlots[i];
                m_realTraySlots[i] = "";
            }
        }
    }
    
    // 更新所有滑槽标签显示（标签索引1-21对应数组索引0-20）
    for (int i = 0; i < 21 && i < m_realTraySlots.size(); ++i) {
        int labelIndex = i + 1; // 标签索引 = 数组索引 + 1（跳过入口标签0）
        if (labelIndex < m_realTrayLabels.size() && m_realTrayLabels[labelIndex]) {
            m_realTrayLabels[labelIndex]->setText(m_realTraySlots[i]);
        }
    }
    
    // 清除出口标签（如果之前有显示）
    if ((m_realTraySlots.size() <= 20 || m_realTraySlots[20].isEmpty()) && 
        m_realTrayLabels.size() > 22 && m_realTrayLabels[22]) {
        m_realTrayLabels[22]->setText(m_realTrayExitLabelText);
    }
    
    // 保存可视化记录到数据库
    saveVisualizationRecords();
}

/**
 * @brief 保存可视化记录到数据库
 */
void tcpClient::saveVisualizationRecords()
{
    // 检查数据库是否已打开
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "数据库未打开，跳过保存可视化记录";
        return;
    }
    
    // 检查是否有数据需要保存（先从数组检查，因为数组可能比标签更新得更早）
    // 检查位置0-20（21个槽位）
    bool hasData = false;
    for (int i = 0; i < m_realTraySlots.size() && i < 21; ++i) {
        if (!m_realTraySlots[i].isEmpty()) {
            hasData = true;
            break;
        }
    }
    
    // 如果数组没有数据，再从标签检查（标签索引1-21对应数组索引0-20）
    if (!hasData) {
        for (int i = 1; i <= 21 && i < m_realTrayLabels.size(); ++i) {
            if (m_realTrayLabels[i]) {
                QString labelText = m_realTrayLabels[i]->text().trimmed();
                if (!labelText.isEmpty()) {
                    hasData = true;
                    break;
                }
            }
        }
    }
    
    // 也检查出口标签（标签索引22）
    if (!hasData && m_realTrayLabels.size() > 22 && m_realTrayLabels[22]) {
        QString exitText = m_realTrayLabels[22]->text().trimmed();
        if (!exitText.isEmpty() && exitText != m_realTrayExitLabelText) {
            hasData = true;
        }
    }
    
    // 如果没有数据，不执行保存操作，避免清空数据库
    if (!hasData) {
        qDebug() << "没有可视化记录需要保存，跳过保存操作";
        return;
    }
    
    QSqlQuery query;
    
    // 尝试开始事务（如果失败，继续执行，不使用事务）
    bool useTransaction = db.transaction();
    if (!useTransaction) {
        qDebug() << "开始事务失败，将不使用事务保存: " << db.lastError().text();
    }
    
    // 先清空旧记录
    if (!query.exec("DELETE FROM visualization_records")) {
        if (useTransaction) {
            db.rollback();
        }
        appendToLog("清空可视化记录表失败: " + query.lastError().text(), true);
        return;
    }
    
    // 插入当前所有槽位的记录（从标签读取，确保与显示一致）
    // 保存位置1-21（21个槽位，标签索引1-21对应数组索引0-20）
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    query.prepare("INSERT INTO visualization_records (slot_position, vehicle_name, update_time) VALUES (?, ?, ?)");
    
    int savedCount = 0;
    for (int i = 0; i < 21; ++i) {
        int labelIndex = i + 1; // 标签索引 = 数组索引 + 1（跳过入口标签0）
        QString vehicleName = "";
        
        // 从标签读取数据
        if (labelIndex < m_realTrayLabels.size() && m_realTrayLabels[labelIndex]) {
            vehicleName = m_realTrayLabels[labelIndex]->text().trimmed();
        }
        
        // 如果标签为空，也从数组读取（作为备份）
        if (vehicleName.isEmpty() && i < m_realTraySlots.size()) {
            vehicleName = m_realTraySlots[i].trimmed();
        }
        
        if (!vehicleName.isEmpty()) {
            query.addBindValue(i + 1); // 槽位位置（1-21）
            query.addBindValue(vehicleName);
            query.addBindValue(currentTime);
            if (!query.exec()) {
                if (useTransaction) {
                    db.rollback();
                }
                appendToLog(QString("保存可视化记录失败（槽位%1）: %2").arg(i + 1).arg(query.lastError().text()), true);
                return;
            }
            savedCount++;
            // 同步到数组
            if (i < m_realTraySlots.size()) {
                m_realTraySlots[i] = vehicleName;
            }
        }
    }
    
    // 提交事务（如果使用了事务）
    if (useTransaction) {
        if (!db.commit()) {
            appendToLog("提交事务失败: " + db.lastError().text(), true);
            return;
        }
    }
    
    qDebug() << "可视化记录已保存到数据库，共保存" << savedCount << "条记录";
    appendToLog(QString("可视化记录已保存到数据库，共保存%1条记录").arg(savedCount), false);
}

/**
 * @brief 从数据库加载可视化记录
 */
void tcpClient::loadVisualizationRecords()
{
    // 检查数据库是否已打开
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "数据库未打开，跳过加载可视化记录";
        return;
    }
    
    // 确保数组已初始化
    if (m_realTraySlots.size() != 21) {
        m_realTraySlots.resize(21);
        for (int i = 0; i < 21; ++i) {
            m_realTraySlots[i] = "";
        }
    }
    
    // 先清空当前数组
    for (int i = 0; i < m_realTraySlots.size(); ++i) {
        m_realTraySlots[i] = "";
    }
    
    QSqlQuery query("SELECT slot_position, vehicle_name FROM visualization_records ORDER BY slot_position");
    
    if (query.lastError().isValid()) {
        qDebug() << "查询可视化记录失败:" << query.lastError().text();
        appendToLog(QString("查询可视化记录失败: %1").arg(query.lastError().text()), true);
        return;
    }
    
    int loadedCount = 0;
    while (query.next()) {
        int slotPosition = query.value(0).toInt();
        QString vehicleName = query.value(1).toString();
        
        // 槽位位置是1-21，数组索引是0-20（槽位位置 = 数组索引 + 1）
        if (slotPosition >= 1 && slotPosition <= 21) {
            int arrayIndex = slotPosition - 1;
            if (arrayIndex < m_realTraySlots.size()) {
                m_realTraySlots[arrayIndex] = vehicleName;
                loadedCount++;
                qDebug() << "加载可视化记录: 槽位" << slotPosition << "=" << vehicleName;
            }
        }
    }
    
    qDebug() << "从数据库加载了" << loadedCount << "条可视化记录";
    appendToLog(QString("从数据库加载了%1条可视化记录").arg(loadedCount), false);
    
    // 更新标签显示（确保标签数组已创建）
    qDebug() << "标签数组大小:" << m_realTrayLabels.size();
    if (m_realTrayLabels.size() >= 23) {
        // 更新位置0-20的标签（标签索引1-21对应数组索引0-20）
        for (int i = 0; i < 21 && i < m_realTraySlots.size(); ++i) {
            int labelIndex = i + 1; // 标签索引 = 数组索引 + 1（跳过入口标签0）
            if (labelIndex < m_realTrayLabels.size() && m_realTrayLabels[labelIndex]) {
                m_realTrayLabels[labelIndex]->setText(m_realTraySlots[i]);
                if (!m_realTraySlots[i].isEmpty()) {
                    qDebug() << "更新标签" << labelIndex << "(槽位" << (i + 1) << ")=" << m_realTraySlots[i];
                }
            }
        }
        
        // 确保出口标签显示初始文本（清空车型），不写入索引20的内容
        if (m_realTrayLabels.size() > 22 && m_realTrayLabels[22]) {
            m_realTrayLabels[22]->setText(m_realTrayExitLabelText);
        }
    } else {
        qWarning() << "标签数组未完全初始化，大小=" << m_realTrayLabels.size() << "，期望>=23";
        appendToLog(QString("标签数组未完全初始化，大小=%1，期望>=23").arg(m_realTrayLabels.size()), true);
    }
    
    qDebug() << "可视化记录已从数据库加载";
}

/**
 * @brief 保存连接配置到数据库
 * @param configType 配置类型（"plc" 或 "server"）
 * @param ip IP地址
 * @param port 端口号
 */
void tcpClient::saveConnectionConfig(const QString &configType, const QString &ip, int port)
{
    // 检查数据库是否已打开
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "数据库未打开，跳过保存连接配置";
        return;
    }
    
    QSqlQuery query;
    
    // 先删除该类型的旧配置
    query.prepare("DELETE FROM connection_configs WHERE config_type = ?");
    query.addBindValue(configType);
    if (!query.exec()) {
        appendToLog(QString("删除旧连接配置失败: %1").arg(query.lastError().text()), true);
        return;
    }
    
    // 插入新配置
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    query.prepare("INSERT INTO connection_configs (config_type, ip_address, port, update_time) VALUES (?, ?, ?, ?)");
    query.addBindValue(configType);
    query.addBindValue(ip);
    query.addBindValue(port);
    query.addBindValue(currentTime);
    
    if (!query.exec()) {
        appendToLog(QString("保存连接配置失败: %1").arg(query.lastError().text()), true);
    } else {
        appendToLog(QString("连接配置已保存: %1 - %2:%3").arg(configType).arg(ip).arg(port), false);
    }
}

/**
 * @brief 从数据库加载连接配置
 */
void tcpClient::loadConnectionConfig()
{
    // 检查数据库是否已打开
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "数据库未打开，跳过加载连接配置";
        return;
    }
    
    QSqlQuery query("SELECT config_type, ip_address, port FROM connection_configs");
    
    if (query.lastError().isValid()) {
        qDebug() << "查询连接配置失败:" << query.lastError().text();
        return;
    }
    
    while (query.next()) {
        QString configType = query.value(0).toString();
        QString ip = query.value(1).toString();
        int port = query.value(2).toInt();
        
        if (configType == "plc") {
            // 加载PLC连接配置
            if (!ip.isEmpty()) {
                ui->lineEditServerIP->setText(ip);
            }
            if (port > 0) {
                ui->lineEditPort->setText(QString::number(port));
            }
        } else if (configType == "server") {
            // 加载服务端连接配置
            if (!ip.isEmpty() && lineEditServerIP) {
                lineEditServerIP->setText(ip);
            }
            if (port > 0 && lineEditServerPort) {
                lineEditServerPort->setText(QString::number(port));
            }
        } else if (configType == "ed_software") {
            // 加载ED软件连接配置
            if (!ip.isEmpty() && lineEditEdSoftwareIP) {
                lineEditEdSoftwareIP->setText(ip);
            }
            if (port > 0 && lineEditEdSoftwarePort) {
                lineEditEdSoftwarePort->setText(QString::number(port));
            }
        }
    }
    
    qDebug() << "连接配置已从数据库加载";
}

/**
 * @brief 保存PLC连接配置按钮点击处理
 */
void tcpClient::onSavePlcConnectionConfigClicked()
{
    QString ip = ui->lineEditServerIP->text().trimmed();
    QString portStr = ui->lineEditPort->text().trimmed();
    
    // 验证输入
    if (ip.isEmpty()) {
        QMessageBox::warning(this, "配置错误", "PLC服务器IP地址不能为空！");
        return;
    }
    
    bool ok;
    int port = portStr.toInt(&ok);
    if (!ok || port <= 0 || port > 65535) {
        QMessageBox::warning(this, "配置错误", "端口号必须是1-65535之间的数字！");
        return;
    }
    
    // 保存到数据库
    saveConnectionConfig("plc", ip, port);
    QMessageBox::information(this, "保存成功", QString("PLC连接配置已保存！\n\nIP: %1\n端口: %2").arg(ip).arg(port));
}

/**
 * @brief 保存服务端连接配置按钮点击处理
 */
void tcpClient::onSaveServerConnectionConfigClicked()
{
    if (!lineEditServerIP || !lineEditServerPort) {
        QMessageBox::warning(this, "错误", "服务端连接设置未初始化！");
        return;
    }
    
    QString ip = lineEditServerIP->text().trimmed();
    QString portStr = lineEditServerPort->text().trimmed();
    
    // 验证输入
    if (ip.isEmpty()) {
        QMessageBox::warning(this, "配置错误", "服务端IP地址不能为空！");
        return;
    }
    
    bool ok;
    int port = portStr.toInt(&ok);
    if (!ok || port <= 0 || port > 65535) {
        QMessageBox::warning(this, "配置错误", "端口号必须是1-65535之间的数字！");
        return;
    }
    
    // 保存到数据库
    saveConnectionConfig("server", ip, port);
    QMessageBox::information(this, "保存成功", QString("服务端连接配置已保存！\n\nIP: %1\n端口: %2").arg(ip).arg(port));
}

/**
 * @brief 服务端连接按钮点击处理
 */
void tcpClient::onServerConnectClicked()
{
    if (!lineEditServerIP || !lineEditServerPort) {
        QMessageBox::warning(this, "错误", "服务端连接设置未初始化！");
        return;
    }
    
    QString serverIP = lineEditServerIP->text().trimmed();
    QString portStr = lineEditServerPort->text().trimmed();

    qInfo() << "用户尝试连接服务端:" << serverIP << ":" << portStr;

    // 验证IP地址
    if (serverIP.isEmpty()) {
        QString errorMsg = "请输入服务端IP地址";
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
    QString connectMsg = QString("正在连接到服务端 %1:%2...").arg(serverIP).arg(port);
    appendToLog(connectMsg);
    qInfo() << connectMsg;

    m_serverSocket->connectToHost(serverIP, port);
    m_serverConnectionTimer->start();

    pushButtonServerConnect->setEnabled(false);
}

/**
 * @brief 服务端断开按钮点击处理
 */
void tcpClient::onServerDisconnectClicked()
{
    if (m_serverSocket->state() == QAbstractSocket::ConnectedState) {
        m_serverSocket->disconnectFromHost();
        appendToLog("正在断开服务端连接...");
    }
}

/**
 * @brief 服务端Socket连接成功处理
 */
void tcpClient::onServerSocketConnected()
{
    m_serverConnectionTimer->stop();
    m_isServerConnected = true;
    updateServerConnectionStatus(true);
    
    // 启动可视化数据发送定时器
    m_visualizationDataTimer->start();
    appendToLog("数据发送定时器已启动（每3秒触发一次，1秒间隔上报AGT搬运、工程组和异常记录数据）", false);
    
    // 立即发送一次AGT搬运数据
    sendVisualizationDataToServer();
    
    // 启动工程组数据发送定时器（3秒后发送）
    m_projectGroupDataTimer->start();
    
    QString successMsg = "服务端连接成功！";
    appendToLog(successMsg, false);
    qInfo() << successMsg << "服务端地址:" << m_serverSocket->peerAddress().toString() << "端口:" << m_serverSocket->peerPort();
}

/**
 * @brief 服务端Socket断开连接处理
 */
void tcpClient::onServerSocketDisconnected()
{
    m_serverConnectionTimer->stop();
    m_isServerConnected = false;
    updateServerConnectionStatus(false);
    
    // 停止数据发送定时器
    m_visualizationDataTimer->stop();
    m_projectGroupDataTimer->stop();
    m_exceptionDataTimer->stop();
    
    appendToLog("服务端连接已断开", false);
}

/**
 * @brief 服务端Socket错误处理
 * @param error 错误类型
 */
void tcpClient::onServerSocketError(QAbstractSocket::SocketError error)
{
    m_serverConnectionTimer->stop();
    m_isServerConnected = false;
    updateServerConnectionStatus(false);

    QString errorMsg = m_serverSocket->errorString();
    appendToLog(QString("服务端连接错误: %1").arg(errorMsg), true);
}

/**
 * @brief 服务端Socket数据可读处理
 */
void tcpClient::onServerSocketReadyRead()
{
    QByteArray data = m_serverSocket->readAll();
    appendToLog(QString("收到服务端数据: %1 字节").arg(data.size()), false);
    qDebug() << "服务端数据:" << data;
    
    // 处理JSON数据
    processServerJsonData(data);
}

/**
 * @brief 服务端连接超时处理
 */
void tcpClient::onServerConnectionTimeout()
{
    m_serverSocket->abort();
    appendToLog("服务端连接超时", true);
    updateServerConnectionStatus(false);
}

/**
 * @brief 更新服务端连接状态显示
 * @param connected 连接状态
 */
void tcpClient::updateServerConnectionStatus(bool connected)
{
    if (!labelServerConnectionStatus || !pushButtonServerConnect || !pushButtonServerDisconnect) {
        return;
    }
    
    if (connected) {
        labelServerConnectionStatus->setText("已连接");
        pushButtonServerConnect->setEnabled(false);
        pushButtonServerDisconnect->setEnabled(true);
        if (lineEditServerIP) lineEditServerIP->setEnabled(false);
        if (lineEditServerPort) lineEditServerPort->setEnabled(false);
    } else {
        labelServerConnectionStatus->setText("未连接");
        pushButtonServerConnect->setEnabled(true);
        pushButtonServerDisconnect->setEnabled(false);
        if (lineEditServerIP) lineEditServerIP->setEnabled(true);
        if (lineEditServerPort) lineEditServerPort->setEnabled(true);
    }
    
    // 更新右下角状态标签
    if (m_labelServerStatus) {
        if (connected) {
            m_labelServerStatus->setText("服务端: 已连接");
            m_labelServerStatus->setStyleSheet("QLabel { color: green; font-weight: bold; }");
        } else {
            m_labelServerStatus->setText("服务端: 未连接");
            m_labelServerStatus->setStyleSheet("QLabel { color: red; font-weight: bold; }");
        }
    }
}

/**
 * @brief 处理单个JSON对象
 * @param jsonString JSON字符串
 * @param saveToTable 是否保存到数据表格和当前班次表格（默认true）
 * @return 是否处理成功
 */
bool tcpClient::processSingleJsonObject(const QString &jsonString, bool saveToTable)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        appendToLog(QString("JSON解析错误: %1, 错误位置: %2").arg(error.errorString()).arg(error.offset), true);
        return false;
    }
    
    if (!doc.isObject()) {
        appendToLog("JSON数据格式错误: 不是对象", true);
        return false;
    }
    
    QJsonObject obj = doc.object();
    QString instructionType = obj.value("instructionType").toString();
    
    // 只处理"AGT滑槽指令"
    if (instructionType != "AGT滑槽指令") {
        appendToLog(QString("忽略非AGT滑槽指令: %1").arg(instructionType), false);
        return false;
    }
    
    QString status = obj.value("status").toString();
    QString modelName = obj.value("modelName").toString();
    QString modelCode = obj.value("modelCode").toString();
    int slotNumber = obj.value("slotNumber").toInt();
    QString timestamp = obj.value("timestamp").toString();
    
    // 映射为实际滑槽号：1->2001, 2->2002, 3->2003, 4->2103, 5->2102, 6->2101
    int actualSlotNumber = mapSlotNumberToActualSlot(slotNumber);
    
    appendToLog(QString("收到AGT滑槽指令: 状态=%1, 车型=%2, 车型代码=%3, 槽位=%4 (实际滑槽号=%5)")
                .arg(status).arg(modelName).arg(modelCode).arg(slotNumber).arg(actualSlotNumber), false);
    
    // 获取当前时间（如果timestamp为空，使用当前时间）
    QString currentTime = timestamp;
    if (currentTime.isEmpty()) {
        currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    }
    
    // 确定数量（空托盘的数量为0）
    int count = 0;
    if (status == "实托盘搬入" || status == "实托盘搬出") {
        // 实托盘需要从车型绑定表中获取数量
        for (int row = 0; row < ui->tableWidgetVehicleBinding->rowCount(); ++row) {
            QTableWidgetItem* codeItem = ui->tableWidgetVehicleBinding->item(row, 1); // 车型代码列
            if (codeItem && codeItem->text() == modelCode) {
                QTableWidgetItem* countItem = ui->tableWidgetVehicleBinding->item(row, 2); // 数量列
                if (countItem) {
                    count = countItem->text().toInt();
                    break;
                }
            }
        }
    }
    
    // 给数据表格添加记录（只有当saveToTable为true时才添加）
    if (saveToTable && !modelName.isEmpty() && !modelCode.isEmpty()) {
        int tableRow = ui->tableWidget->rowCount();
        ui->tableWidget->insertRow(tableRow);
        
        // 滑槽号（使用映射后的实际滑槽号）
        QTableWidgetItem* item0 = new QTableWidgetItem(QString::number(actualSlotNumber));
        item0->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(tableRow, 0, item0);
        
        // 状态
        QTableWidgetItem* item1 = new QTableWidgetItem(status);
        item1->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(tableRow, 1, item1);
        
        // 车型代码
        QTableWidgetItem* item2 = new QTableWidgetItem(modelCode);
        item2->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(tableRow, 2, item2);
        
        // 车型名称
        QTableWidgetItem* item3 = new QTableWidgetItem(modelName);
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
        
        // 保存到数据库（使用映射后的实际滑槽号）
        insertDataRecord(actualSlotNumber, status, modelName, modelCode, count, currentTime);
        
        // 给当前班次表格添加记录（使用映射后的实际滑槽号）
        addDataToCurrentShiftTable(actualSlotNumber, status, modelCode, modelName, count, currentTime);

        appendToLog(QString("滑槽指令数据已添加到表格 - %1, 车型: %2").arg(status).arg(modelName), false);
        
        // 如果工程组页面可见且显示当前班次，更新工程组统计表格
        if (ui->stackedWidget->currentIndex() == 4 && m_projectGroupDisplayShift == "current") {
            updateProjectGroupStatistics();
        }
    }
    
    // 根据状态处理可视化界面
    if (status == "实托盘搬入") {
        handleRealTrayIn(modelName, slotNumber);
    } else if (status == "空托盘搬出") {
        handleEmptyTrayOut(modelName, slotNumber);
    } else {
        appendToLog(QString("未处理的状态: %1").arg(status), false);
    }
    
    return true;
}

/**
 * @brief 处理服务端JSON数据（支持多个连续的JSON对象）
 * @param data JSON数据
 * @param saveToTable 是否保存到数据表格和当前班次表格（默认true）
 */
void tcpClient::processServerJsonData(const QByteArray &data, bool saveToTable)
{
    // 清理数据：移除可能的控制字符和空白字符
    QByteArray cleanedData = data;
    
    // 移除开头的反引号、单引号或其他控制字符
    while (!cleanedData.isEmpty()) {
        char firstChar = cleanedData[0];
        if (firstChar == '`' || firstChar == '\'' || firstChar == '\0' || 
            (firstChar < 0x20 && firstChar != '\n' && firstChar != '\r' && firstChar != '\t')) {
            cleanedData.remove(0, 1);
        } else {
            break;
        }
    }
    
    // 移除末尾的控制字符
    while (!cleanedData.isEmpty()) {
        char lastChar = cleanedData[cleanedData.size() - 1];
        if (lastChar == '\0' || (lastChar < 0x20 && lastChar != '\n' && lastChar != '\r' && lastChar != '\t')) {
            cleanedData.remove(cleanedData.size() - 1, 1);
        } else {
            break;
        }
    }
    
    // 转换为QString，尝试使用UTF-8编码
    QString jsonString = QString::fromUtf8(cleanedData);
    
    // 如果UTF-8解析失败或包含乱码，尝试使用本地编码（Windows中文系统通常是GBK）
    if (jsonString.contains(QChar::ReplacementCharacter) || 
        (jsonString.isEmpty() && !cleanedData.isEmpty())) {
        // 尝试使用本地编码（在Windows中文系统上通常是GBK）
        jsonString = QString::fromLocal8Bit(cleanedData);
        qDebug() << "使用本地编码解析JSON数据";
        
        // 如果本地编码也失败，尝试直接使用Latin1然后转换
        if (jsonString.contains(QChar::ReplacementCharacter)) {
            jsonString = QString::fromLatin1(cleanedData);
            qDebug() << "使用Latin1编码解析JSON数据";
        }
    }
    
    // 记录清理后的数据用于调试
    qDebug() << "清理后的JSON数据:" << jsonString;
    qDebug() << "清理后的JSON数据(hex):" << cleanedData.toHex(' ');
    
    // 检测并分割多个连续的JSON对象（通过查找 "}{" 模式）
    QStringList jsonObjects;
    int startPos = 0;
    int braceCount = 0;
    bool inString = false;
    bool escapeNext = false;
    
    for (int i = 0; i < jsonString.length(); ++i) {
        QChar ch = jsonString[i];
        
        if (escapeNext) {
            escapeNext = false;
            continue;
        }
        
        if (ch == '\\') {
            escapeNext = true;
            continue;
        }
        
        if (ch == '"') {
            inString = !inString;
            continue;
        }
        
        if (!inString) {
            if (ch == '{') {
                if (braceCount == 0) {
                    startPos = i;
                }
                braceCount++;
            } else if (ch == '}') {
                braceCount--;
                if (braceCount == 0) {
                    // 找到一个完整的JSON对象
                    QString jsonObj = jsonString.mid(startPos, i - startPos + 1);
                    jsonObjects.append(jsonObj);
                }
            }
        }
    }
    
    // 如果没有找到多个对象，尝试作为单个对象处理
    if (jsonObjects.isEmpty()) {
        jsonObjects.append(jsonString);
    }
    
    // 处理每个JSON对象
    int successCount = 0;
    for (int i = 0; i < jsonObjects.size(); ++i) {
        if (processSingleJsonObject(jsonObjects[i], saveToTable)) {
            successCount++;
        }
    }
    
    if (jsonObjects.size() > 1) {
        appendToLog(QString("处理了 %1 个JSON对象，成功 %2 个").arg(jsonObjects.size()).arg(successCount), false);
    }
}

/**
 * @brief 处理实托盘搬入
 * 如果指定了slotNumber，根据slotNumber确定要检查的槽位（最靠近出口的3个槽）
 * slotNumber=1或4 -> 第一个槽（索引20），slotNumber=2或5 -> 第二个槽（索引19），slotNumber=3或6 -> 第三个槽（索引18）
 * 如果未指定slotNumber，从出口开始向前查找，找到最靠近出口且不为空的槽位
 * 匹配该位置的车型，如果匹配就清空该槽位，不匹配则跳过
 */
void tcpClient::handleRealTrayIn(const QString &modelName, int slotNumber)
{
    QString trimmedModel = modelName.trimmed();
    
    // 如果指定了slotNumber，先从出口往前找，找到最靠近出口且有车型的一行（3个连续槽位）
    if (slotNumber > 0) {
        // 确定slotNumber对应的位置（1、2、3）
        int positionInRow = -1;
        if (slotNumber == 1 || slotNumber == 4) {
            positionInRow = 0; // 第一个位置
        } else if (slotNumber == 2 || slotNumber == 5) {
            positionInRow = 1; // 第二个位置
        } else if (slotNumber == 3 || slotNumber == 6) {
            positionInRow = 2; // 第三个位置
        }
        
        if (positionInRow >= 0) {
            // 从出口往前找，找到最靠近出口且有车型的一行（3个连续槽位）
            // 最靠近出口的一行是索引18、19、20，如果没有车型，继续往前找（15、16、17等）
            int rowStartIndex = -1;
            
            // 从最靠近出口的一行开始（索引18、19、20），往前查找
            for (int startIdx = 18; startIdx >= 0; startIdx -= 3) {
                // 检查这一行（3个连续槽位）是否至少有一个有车型
                bool hasVehicleInRow = false;
                
                // 检查这一行的3个槽位
                for (int offset = 0; offset < 3 && (startIdx + offset) <= 20; ++offset) {
                    int slotIndex = startIdx + offset;
                    int labelIndex = slotIndex + 1;
                    
                    // 如果槽位是索引20，先检查出口标签
                    if (slotIndex == 20 && m_realTrayLabels.size() > 22 && m_realTrayLabels[22]) {
                        QString exitText = m_realTrayLabels[22]->text().trimmed();
                        if (!exitText.isEmpty() && exitText != m_realTrayExitLabelText) {
                            hasVehicleInRow = true;
                            break;
                        }
                    }
                    
                    // 检查普通槽位标签
                    if (labelIndex < m_realTrayLabels.size() && m_realTrayLabels[labelIndex]) {
                        QString labelText = m_realTrayLabels[labelIndex]->text().trimmed();
                        if (!labelText.isEmpty()) {
                            hasVehicleInRow = true;
                            break;
                        }
                    }
                }
                
                // 如果这一行有车型，使用这一行
                if (hasVehicleInRow) {
                    rowStartIndex = startIdx;
                    break;
                }
            }
            
            // 如果找到了有车型的一行
            if (rowStartIndex >= 0 && rowStartIndex <= 18) {
                // 计算目标槽位索引（这一行的第positionInRow个位置）
                int targetSlotIndex = rowStartIndex + positionInRow;
                
                if (targetSlotIndex >= 0 && targetSlotIndex <= 20) {
                    int labelIndex = targetSlotIndex + 1; // 标签索引 = 数组索引 + 1（标签0是入口）
                    
                    // 如果目标槽位是索引20，先检查出口标签
                    if (targetSlotIndex == 20 && m_realTrayLabels.size() > 22 && m_realTrayLabels[22]) {
                        QString exitText = m_realTrayLabels[22]->text().trimmed();
                        if (!exitText.isEmpty() && exitText != m_realTrayExitLabelText) {
                            // 提取车型名称（去除初始文本前缀）
                            QString vehicleName = exitText;
                            if (vehicleName.startsWith(m_realTrayExitLabelText)) {
                                vehicleName = vehicleName.mid(vehicleName.indexOf("\n") + 1).trimmed();
                            }
                            
                            if (!vehicleName.isEmpty()) {
                                if (vehicleName == trimmedModel) {
                                    // 匹配成功，清空出口标签
                                    m_realTrayLabels[22]->setText(m_realTrayExitLabelText);
                                    // 清空对应的数组（最后一个槽位，索引20）
                                    if (m_realTraySlots.size() > 20) {
                                        m_realTraySlots[20] = "";
                                    }
                                    saveVisualizationRecords();
                                    
                                    // 增加实际便次
                                    m_actualCount++;
                                    if (m_currentDisplayShift == "current" && actualCountLabel) {
                                        actualCountLabel->setText(QString("实际便次：%1便").arg(m_actualCount));
                                    }
                                    m_displayedActualCount = m_actualCount; // 更新显示值
                                    saveStatisticsInfo(); // 保存到数据库
                                    
                                    appendToLog(QString("实托盘搬入: 匹配到槽位%1（slotNumber=%2，位置%3）的车型%4，已清空，实际便次+1")
                                               .arg(targetSlotIndex + 1).arg(slotNumber).arg(positionInRow + 1).arg(modelName), false);
                                    return;
                                } else {
                                    // 车型不匹配，记录异常
                                    int actualSlotNo = mapSlotNumberToActualSlot(slotNumber);
                                    QString currentDate = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
                                    insertExceptionRecord(actualSlotNo, modelName, "实托盘搬入", "出口车型不匹配", currentDate);
                                    appendToLog(QString("实托盘搬入: 槽位%1（slotNumber=%2，位置%3）的车型%4与%5不匹配，已记录异常").arg(targetSlotIndex + 1).arg(slotNumber).arg(positionInRow + 1).arg(vehicleName).arg(modelName), false);
                                    return;
                                }
                            }
                        }
                    }
                    
                    // 检查目标槽位的标签
                    if (labelIndex < m_realTrayLabels.size() && m_realTrayLabels[labelIndex]) {
                        QString labelText = m_realTrayLabels[labelIndex]->text().trimmed();
                        
                        if (!labelText.isEmpty()) {
                            if (labelText == trimmedModel) {
                                // 匹配成功，清空该槽位
                                if (targetSlotIndex < m_realTraySlots.size()) {
                                    m_realTraySlots[targetSlotIndex] = "";
                                }
                                m_realTrayLabels[labelIndex]->setText("");
                                
                                // 保存到数据库
                                saveVisualizationRecords();
                                
                                // 增加实际便次
                                m_actualCount++;
                                if (m_currentDisplayShift == "current" && actualCountLabel) {
                                    actualCountLabel->setText(QString("实际便次：%1便").arg(m_actualCount));
                                }
                                m_displayedActualCount = m_actualCount; // 更新显示值
                                saveStatisticsInfo(); // 保存到数据库
                                
                                appendToLog(QString("实托盘搬入: 匹配到槽位%1（slotNumber=%2，位置%3）的车型%4，已清空，实际便次+1")
                                           .arg(targetSlotIndex + 1).arg(slotNumber).arg(positionInRow + 1).arg(modelName), false);
                                return;
                            } else {
                                // 车型不匹配，记录异常
                                int actualSlotNo = mapSlotNumberToActualSlot(slotNumber);
                                QString currentDate = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
                                insertExceptionRecord(actualSlotNo, modelName, "实托盘搬入", "出口车型不匹配", currentDate);
                                appendToLog(QString("实托盘搬入: 槽位%1（slotNumber=%2，位置%3）的车型%4与%5不匹配，已记录异常").arg(targetSlotIndex + 1).arg(slotNumber).arg(positionInRow + 1).arg(labelText).arg(modelName), false);
                                return;
                            }
                        } else {
                            // 目标槽位为空，跳过
                            appendToLog(QString("实托盘搬入: 槽位%1（slotNumber=%2，位置%3）为空，跳过")
                                       .arg(targetSlotIndex + 1).arg(slotNumber).arg(positionInRow + 1), false);
                            return;
                        }
                    }
                }
            } else {
                // 没有找到有车型的一行，跳过
                appendToLog(QString("实托盘搬入: 未找到最靠近出口且有车型的一行（slotNumber=%1），跳过").arg(slotNumber), false);
                return;
            }
        }
    }
    
    // 如果未指定slotNumber或slotNumber无效，使用原来的逻辑
    // 首先检查出口标签（索引22），这是最靠近出口的位置
    if (m_realTrayLabels.size() > 22 && m_realTrayLabels[22]) {
        QString exitText = m_realTrayLabels[22]->text().trimmed();
        // 如果出口标签包含车型信息（格式可能是"初始文本\n车型"或只有车型）
        if (!exitText.isEmpty() && exitText != m_realTrayExitLabelText) {
            // 提取车型名称（去除初始文本前缀）
            QString vehicleName = exitText;
            if (vehicleName.startsWith(m_realTrayExitLabelText)) {
                vehicleName = vehicleName.mid(vehicleName.indexOf("\n") + 1).trimmed();
            }
            
            if (!vehicleName.isEmpty()) {
                // 找到最靠近出口且不为空的位置（出口标签）
                if (vehicleName == trimmedModel) {
                    // 匹配成功，清空出口标签
                    m_realTrayLabels[22]->setText(m_realTrayExitLabelText);
                    // 清空对应的数组（最后一个槽位，索引20）
                    if (m_realTraySlots.size() > 20) {
                        m_realTraySlots[20] = "";
                    }
                    saveVisualizationRecords();
                    
                    // 增加实际便次
                    m_actualCount++;
                    if (m_currentDisplayShift == "current" && actualCountLabel) {
                        actualCountLabel->setText(QString("实际便次：%1便").arg(m_actualCount));
                    }
                    m_displayedActualCount = m_actualCount; // 更新显示值
                    saveStatisticsInfo(); // 保存到数据库
                    
                    appendToLog(QString("实托盘搬入: 匹配到出口的车型%1，已清空，实际便次+1").arg(modelName), false);
                    return;
                } else {
                    // 出口位置的车型不匹配，记录异常（出口位置对应索引20，滑槽号2103）
                    QString currentDate = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
                    insertExceptionRecord(2103, modelName, "实托盘搬入", "出口车型不匹配", currentDate);
                    appendToLog(QString("实托盘搬入: 最靠近出口的车型%1与%2不匹配，已记录异常").arg(vehicleName).arg(modelName), false);
                    return;
                }
            }
        }
    }
    
    // 如果出口标签为空，从槽位21开始向前查找，找到第一个不为空的槽位
    // 从标签读取数据，确保与显示一致（标签索引21-1对应数组索引20-0）
    for (int i = 20; i >= 0; --i) {
        int labelIndex = i + 1; // 标签索引 = 数组索引 + 1（标签0是入口）
        
        if (labelIndex < m_realTrayLabels.size() && m_realTrayLabels[labelIndex]) {
            QString labelText = m_realTrayLabels[labelIndex]->text().trimmed();
            
            // 如果标签不为空，说明该槽位有车型
            if (!labelText.isEmpty()) {
                // 找到最靠近出口且不为空的槽位
                appendToLog(QString("实托盘搬入: 找到最靠近出口的槽位%1，车型=%2").arg(i + 1).arg(labelText), false);
                
                if (labelText == trimmedModel) {
                    // 匹配成功，清空该槽位
                    if (i < m_realTraySlots.size()) {
                        m_realTraySlots[i] = "";
                    }
                    m_realTrayLabels[labelIndex]->setText("");
                    
                    // 保存到数据库
                    saveVisualizationRecords();
                    
                    // 增加实际便次
                    m_actualCount++;
                    if (m_currentDisplayShift == "current" && actualCountLabel) {
                        actualCountLabel->setText(QString("实际便次：%1便").arg(m_actualCount));
                    }
                    m_displayedActualCount = m_actualCount; // 更新显示值
                    saveStatisticsInfo(); // 保存到数据库
                    
                    appendToLog(QString("实托盘搬入: 匹配到槽位%1的车型%2，已清空，实际便次+1").arg(i + 1).arg(modelName), false);
                    return;
                } else {
                    // 最靠近出口且不为空的槽位车型不匹配，记录异常
                    int slotNo = getSlotNoFromIndex(i, true); // 实托盘
                    QString currentDate = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
                    insertExceptionRecord(slotNo, modelName, "实托盘搬入", "出口车型不匹配", currentDate);
                    appendToLog(QString("实托盘搬入: 最靠近出口的槽位%1的车型%2与%3不匹配，已记录异常").arg(i + 1).arg(labelText).arg(modelName), false);
                    return;
                }
            }
        }
    }
    
    // 如果所有槽位都为空，说明没有车型，直接跳过
    appendToLog(QString("实托盘搬入: 所有槽位为空，无法匹配车型%1，跳过").arg(modelName), false);
}

/**
 * @brief 处理空托盘搬入
 * 空托盘是从下往上进入的（从出口向入口方向），所以最上方是入口（索引0-2），最下方是出口（索引18-20）
 * 如果指定了slotNo，从入口往后找，找到最靠近入口且有车型的一行（3个连续槽位）
 * slotNo=4（第8个字节）->2103，对应右边槽位（positionInRow=2），slotNo=5（第9个字节）->2102，对应中间槽位（positionInRow=1），slotNo=6（第10个字节）->2101，对应左边槽位（positionInRow=0）
 * 如果未指定slotNo，从入口开始向后查找，找到最靠近入口且不为空的空托盘槽位
 * 匹配该位置的车型，如果匹配就清空该槽位，不匹配则跳过
 * 注意：入口位置不增加实际便次
 */
void tcpClient::handleEmptyTrayIn(const QString &modelName, int slotNo)
{
    QString trimmedModel = modelName.trimmed();
    
    // 如果指定了slotNo，使用新的逻辑
    if (slotNo >= 4 && slotNo <= 6) {
        // 确定slotNo对应的位置（0、1、2）
        // slotNo=4（第一个字节）->2103，对应右边槽位（positionInRow=2）
        // slotNo=5（第二个字节）->2102，对应中间槽位（positionInRow=1）
        // slotNo=6（第三个字节）->2101，对应左边槽位（positionInRow=0）
        // 注意：空托盘搬入指令的slotNo=4、5、6映射为2103、2102、2101（与实托盘相同）
        int positionInRow = 6 - slotNo; // slotNo=4->2, slotNo=5->1, slotNo=6->0

        // 从入口往后找，找到最靠近入口且有车型的一行（3个连续槽位）
        // 最靠近入口的一行是索引0、1、2，如果没有车型，继续往后找（3、4、5等）
        int rowStartIndex = -1;

        // 从最靠近入口的一行开始（索引0、1、2），往后查找
        for (int startIdx = 0; startIdx <= 18; startIdx += 3) {
            // 检查这一行（3个连续槽位）是否至少有一个有车型
            bool hasVehicleInRow = false;
            
            // 检查这一行的3个槽位
            for (int offset = 0; offset < 3 && (startIdx + offset) <= 20; ++offset) {
                int slotIndex = startIdx + offset;
                int labelIndex = slotIndex + 1;
                
                // 如果槽位是索引20，先检查出口标签
                if (slotIndex == 20 && m_emptyTrayLabels.size() > 22 && m_emptyTrayLabels[22]) {
                    QString exitText = m_emptyTrayLabels[22]->text().trimmed();
                    if (!exitText.isEmpty() && exitText != m_emptyTrayExitLabelText) {
                        hasVehicleInRow = true;
                        break;
                    }
                }
                
                // 检查普通槽位标签
                if (labelIndex < m_emptyTrayLabels.size() && m_emptyTrayLabels[labelIndex]) {
                    QString labelText = m_emptyTrayLabels[labelIndex]->text().trimmed();
                    if (!labelText.isEmpty()) {
                        hasVehicleInRow = true;
                        break;
                    }
                }
            }
            
            // 如果这一行有车型，使用这一行
            if (hasVehicleInRow) {
                rowStartIndex = startIdx;
                break;
            }
        }
        
        // 如果找到了有车型的一行
        if (rowStartIndex >= 0 && rowStartIndex <= 18) {
            // 注意：不再检查出口标签，因为是从入口开始查找
            // 计算目标槽位索引（这一行的第positionInRow个位置）
            int targetSlotIndex = rowStartIndex + positionInRow;
            
            if (targetSlotIndex >= 0 && targetSlotIndex <= 20) {
                int labelIndex = targetSlotIndex + 1; // 标签索引 = 数组索引 + 1（标签0是入口）
                
                
                // 检查目标槽位的标签
                if (labelIndex < m_emptyTrayLabels.size() && m_emptyTrayLabels[labelIndex]) {
                    QString labelText = m_emptyTrayLabels[labelIndex]->text().trimmed();
                    
                    if (!labelText.isEmpty()) {
                        if (labelText == trimmedModel) {
                            // 匹配成功，清空该槽位（不增加实际便次）
                            if (targetSlotIndex < m_emptyTraySlots.size()) {
                                m_emptyTraySlots[targetSlotIndex] = "";
                            }
                            m_emptyTrayLabels[labelIndex]->setText("");
                            
                            // 保存到数据库
                            saveEmptyTrayVisualizationRecords();
                            
                            appendToLog(QString("空托盘搬入: 匹配到槽位%1（slotNo=%2，位置%3）的车型%4，已清空")
                                       .arg(targetSlotIndex + 1).arg(slotNo).arg(positionInRow + 1).arg(modelName), false);
                            return;
                        } else {
                            // 车型不匹配，记录异常
                            // slotNo=4（第一个字节）->2103
                            // slotNo=5（第二个字节）->2102
                            // slotNo=6（第三个字节）->2101
                            int actualSlotNo = mapSlotNumberToActualSlot(slotNo);
                            QString currentDate = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
                            insertExceptionRecord(actualSlotNo, modelName, "空托盘搬入", "入口车型不匹配", currentDate);
                                appendToLog(QString("空托盘搬入: 槽位%1（slotNo=%2，位置%3）的车型%4与%5不匹配，已记录异常").arg(targetSlotIndex + 1).arg(slotNo).arg(positionInRow + 1).arg(labelText).arg(modelName), false);
                            return;
                        }
                    } else {
                        // 目标槽位为空，跳过
                        appendToLog(QString("空托盘搬入: 槽位%1（slotNo=%2，位置%3）为空，跳过")
                                   .arg(targetSlotIndex + 1).arg(slotNo).arg(positionInRow + 1), false);
                        return;
                    }
                }
            }
        } else {
            // 没有找到有车型的一行，跳过
            appendToLog(QString("空托盘搬入: 未找到最靠近入口且有车型的一行（slotNo=%1），跳过").arg(slotNo), false);
            return;
        }
    }
    
    // 如果未指定slotNo或slotNo无效，使用原来的逻辑
    // 从入口开始向后查找，找到第一个不为空的槽位（最靠近入口的位置）
    // 从标签读取数据，确保与显示一致（标签索引1-21对应数组索引0-20）
    for (int i = 0; i <= 20; ++i) {
        int labelIndex = i + 1; // 标签索引 = 数组索引 + 1（跳过入口标签0）
        if (labelIndex < m_emptyTrayLabels.size() && m_emptyTrayLabels[labelIndex]) {
            QString labelText = m_emptyTrayLabels[labelIndex]->text().trimmed();
            
            // 如果标签不为空，说明该槽位有车型
            if (!labelText.isEmpty()) {
                // 找到最靠近入口且不为空的槽位
                appendToLog(QString("空托盘搬入: 找到最靠近入口的槽位%1，车型=%2").arg(i + 1).arg(labelText), false);
                
                if (labelText == trimmedModel) {
                    // 匹配成功，清空该槽位（不增加实际便次）
                    if (i < m_emptyTraySlots.size()) {
                        m_emptyTraySlots[i] = "";
                    }
                    m_emptyTrayLabels[labelIndex]->setText("");
                    
                    // 保存到数据库
                    saveEmptyTrayVisualizationRecords();
                    
                    appendToLog(QString("空托盘搬入: 匹配到槽位%1的车型%2，已清空").arg(i + 1).arg(modelName), false);
                    return;
                } else {
                    // 最靠近入口且不为空的槽位车型不匹配，记录异常
                    int slotNo = getSlotNoFromIndex(i, false); // 空托盘
                    QString currentDate = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
                    insertExceptionRecord(slotNo, modelName, "空托盘搬入", "入口车型不匹配", currentDate);
                    appendToLog(QString("空托盘搬入: 最靠近入口的槽位%1的车型%2与%3不匹配，已记录异常").arg(i + 1).arg(labelText).arg(modelName), false);
                    return;
                }
            }
        }
    }
    
    // 如果所有槽位都为空，说明没有车型，直接跳过
    appendToLog(QString("空托盘搬入: 所有槽位为空，无法匹配车型%1，跳过").arg(modelName), false);
}

/**
 * @brief 处理空托盘搬出
 * 改为从下往上进入（从出口向入口方向，上下顺序跟实滑槽相反）
 * 从左往右进入（左右顺序跟实滑槽相同：2001->2002->2003）
 * 如果指定了slotNumber，在第一次收到时先推进3个槽位，然后根据slotNumber写入对应位置
 * slotNumber=1或4 -> 左边槽位（索引18，列2003），slotNumber=2或5 -> 中间槽位（索引19，列2002），slotNumber=3或6 -> 右边槽位（索引20，列2001）
 * 如果未指定slotNumber，使用原来的逻辑
 */
void tcpClient::handleEmptyTrayOut(const QString &modelName, int slotNumber)
{
    // 如果指定了slotNumber，使用新的逻辑
    if (slotNumber > 0) {
        // 确定slotNumber对应的位置（从下往上，从左往右：18、19、20）
        // 与实滑槽的左右顺序相同（从左往右），但上下顺序相反（从下往上）
        // 左边（col=0）对应2003，数组索引18；中间（col=1）对应2002，数组索引19；右边（col=2）对应2001，数组索引20
        int positionInRow = -1;
        if (slotNumber == 1 || slotNumber == 4) {
            positionInRow = 18; // 左边槽位（列2003，数组索引18）
        } else if (slotNumber == 2 || slotNumber == 5) {
            positionInRow = 19; // 中间槽位（列2002，数组索引19）
        } else if (slotNumber == 3 || slotNumber == 6) {
            positionInRow = 20; // 右边槽位（列2001，数组索引20）
        }
        
        if (positionInRow >= 18 && positionInRow <= 20) {
            // 如果是第一次（批次计数为0），先推进3个位置（从入口向出口方向）
            if (m_emptyTrayBatchCount == 0) {
                advanceEmptyTrayVisualizationBy3(modelName);
            }
            
            // 根据位置索引写入对应的槽位（从下往上）
            if (positionInRow < m_emptyTraySlots.size()) {
                m_emptyTraySlots[positionInRow] = modelName;
                
                // 更新对应槽的标签显示（标签索引positionInRow+1，跳过入口标签0）
                if ((positionInRow + 1) < m_emptyTrayLabels.size() && m_emptyTrayLabels[positionInRow + 1]) {
                    m_emptyTrayLabels[positionInRow + 1]->setText(modelName);
                }
                
                // 更新批次计数
                m_emptyTrayBatchCount = (m_emptyTrayBatchCount + 1) % 3;
                
                // 保存到数据库
                saveEmptyTrayVisualizationRecords();
                
                appendToLog(QString("空托盘搬出: 车型%1已添加到槽位%2（slotNumber=%3，位置%4，从下往上）")
                           .arg(modelName)
                           .arg(positionInRow + 1)
                           .arg(slotNumber)
                           .arg(positionInRow + 1), false);
                return;
            } else {
                appendToLog(QString("空托盘搬出: 无法添加车型到第%1个槽: %2").arg(positionInRow + 1).arg(modelName), true);
                return;
            }
        }
    }
    
    // 如果未指定slotNumber或slotNumber无效，使用原来的逻辑
    // 先检查是否有任何槽位有内容，如果有就需要推进（检查位置0-20，21个槽位）
    bool hasContent = false;
    for (int i = 0; i < m_emptyTraySlots.size() && i < 21; ++i) {
        if (!m_emptyTraySlots[i].isEmpty()) {
            hasContent = true;
            break;
        }
    }
    
    // 如果有内容，先推进所有车型（从入口向出口方向）
    if (hasContent) {
        advanceEmptyTrayVisualization();
    }
    
    // 在最后一个槽位（数组索引20，对应标签索引21，最靠近出口）添加新车型
    if (m_emptyTraySlots.size() > 20) {
        m_emptyTraySlots[20] = modelName;
    }
    
    // 更新标签显示（数组索引20对应标签索引21）
    if (21 < m_emptyTrayLabels.size() && m_emptyTrayLabels[21]) {
        m_emptyTrayLabels[21]->setText(modelName);
    }
    
    appendToLog(QString("空托盘搬出: 车型%1已添加到槽位21（最靠近出口）%2").arg(modelName).arg(hasContent ? "（已推进）" : ""), false);
    
    // 保存到数据库
    saveEmptyTrayVisualizationRecords();
}

/**
 * @brief 推进空托盘可视化显示3个位置
 * 改为从入口向出口方向推进（从下往上进入，所以需要从入口向出口推进）
 * @param modelName 新进入的车型名称（参数保留用于兼容，但异常记录使用被挤出去的车型）
 */
void tcpClient::advanceEmptyTrayVisualizationBy3(const QString &modelName)
{
    // 从入口向出口方向推进3个位置（从左向右）
    // 数组索引0-20对应21个槽位，索引0是入口，索引20是出口
    
    // 先检查21个槽位是否都满了
    bool allSlotsFull = true;
    for (int i = 0; i < 21 && i < m_emptyTraySlots.size(); ++i) {
        if (m_emptyTraySlots[i].isEmpty()) {
            allSlotsFull = false;
            break;
        }
    }
    
    // 先保存当前状态到临时数组，避免覆盖
    QVector<QString> tempSlots = m_emptyTraySlots;
    
    // 如果21个槽位都满了，先清空最靠近入口的3个槽位（索引0、1、2，最靠近入口），不写入到出口标签
    if (allSlotsFull) {
        // 记录异常信息：最靠近入口的3个槽位（索引0、1、2）被挤出去的车型
        // 空托盘从下往上进入，索引0、1、2是入口（最上方），新车型进入时会挤出去这些位置的车型
        // 滑槽号对应：索引0对应2001，索引1对应2002，索引2对应2003
        QString currentDate = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
        
        // 记录索引0的异常（滑槽号2001）- 记录被挤出去的车型名称
        if (tempSlots.size() > 0 && !tempSlots[0].isEmpty()) {
            insertExceptionRecord(2001, tempSlots[0], "空托盘搬入", "槽位满，异常取出", currentDate);
        }
        
        // 记录索引1的异常（滑槽号2002）- 记录被挤出去的车型名称
        if (tempSlots.size() > 1 && !tempSlots[1].isEmpty()) {
            insertExceptionRecord(2002, tempSlots[1], "空托盘搬入", "槽位满，异常取出", currentDate);
        }
        
        // 记录索引2的异常（滑槽号2003）- 记录被挤出去的车型名称
        if (tempSlots.size() > 2 && !tempSlots[2].isEmpty()) {
            insertExceptionRecord(2003, tempSlots[2], "空托盘搬入", "槽位满，异常取出", currentDate);
        }
        
        // 清空最靠近入口的3个槽位（索引0、1、2），为新车型腾出空间
        m_emptyTraySlots[0] = "";
        m_emptyTraySlots[1] = "";
        m_emptyTraySlots[2] = "";
        
        // 确保出口标签保持初始文本，不写入内容
        if (m_emptyTrayLabels.size() > 22 && m_emptyTrayLabels[22]) {
            m_emptyTrayLabels[22]->setText(m_emptyTrayExitLabelText);
        }
        
        appendToLog("空滑槽21个槽位已满，已清空最靠近入口的3个槽位（索引0、1、2），开始推进所有槽位，已记录异常信息", false);
    } else {
        // 如果没满，使用原来的逻辑处理最前面3个槽位
        // 如果索引0有内容，直接清空（不显示在出口标签），因为从下往上进入，最前面的会被推出
        if (tempSlots.size() > 0 && !tempSlots[0].isEmpty()) {
            // 不显示在出口标签，直接清空索引0
            m_emptyTraySlots[0] = "";
        }
        
        // 如果索引1有内容，移动到索引0
        if (tempSlots.size() > 1 && !tempSlots[1].isEmpty()) {
            if (m_emptyTraySlots.size() > 0) {
                m_emptyTraySlots[0] = tempSlots[1];
                m_emptyTraySlots[1] = "";
            }
        }
        
        // 如果索引2有内容，移动到索引1
        if (tempSlots.size() > 2 && !tempSlots[2].isEmpty()) {
            if (m_emptyTraySlots.size() > 1) {
                m_emptyTraySlots[1] = tempSlots[2];
                m_emptyTraySlots[2] = "";
            }
        }
    }
    
    // 从左向右推进3个位置（从索引3到索引20）
    // 这样所有槽位都会从入口向出口方向移动3个位置，为新的数据腾出空间
    for (int i = 3; i <= 20; ++i) {
        if (i < tempSlots.size() && !tempSlots[i].isEmpty()) {
            // 向左移动3格（向入口方向移动）
            int targetIndex = i - 3;
            if (targetIndex >= 0 && targetIndex < m_emptyTraySlots.size()) {
                m_emptyTraySlots[targetIndex] = tempSlots[i];
                m_emptyTraySlots[i] = "";
            }
        }
    }
    
    // 更新所有滑槽标签显示（标签索引1-21对应数组索引0-20）
    for (int i = 0; i < 21 && i < m_emptyTraySlots.size(); ++i) {
        int labelIndex = i + 1; // 标签索引 = 数组索引 + 1（跳过入口标签0）
        if (labelIndex < m_emptyTrayLabels.size() && m_emptyTrayLabels[labelIndex]) {
            m_emptyTrayLabels[labelIndex]->setText(m_emptyTraySlots[i]);
        }
    }
    
    // 清除出口标签（如果索引20为空，确保出口标签保持初始文本）
    if (m_emptyTrayLabels.size() > 22 && m_emptyTrayLabels[22]) {
        if (m_emptyTraySlots.size() <= 20 || m_emptyTraySlots[20].isEmpty()) {
            // 索引20为空，确保出口标签保持初始文本
            m_emptyTrayLabels[22]->setText(m_emptyTrayExitLabelText);
        } else {
            // 如果满槽清空后，索引20可能被移动过来的数据填充，但不应该写入到出口标签
            // 只有在非满槽情况下，索引0的内容才会写入到出口标签（已在上面处理）
        }
    }
    
    // 保存可视化记录到数据库
    saveEmptyTrayVisualizationRecords();
}

/**
 * @brief 推进空托盘可视化显示
 * 改为从入口向出口方向推进（从下往上进入，所以需要从入口向出口推进）
 */
void tcpClient::advanceEmptyTrayVisualization()
{
    // 如果第一个槽位（索引0，最靠近入口）有内容，直接清空（不显示在出口标签）
    // 因为从下往上进入，最前面的会被推出，但不显示在出口标签
    if (m_emptyTraySlots.size() > 0 && !m_emptyTraySlots[0].isEmpty()) {
        m_emptyTraySlots[0] = "";
    }
    
    // 从左到右推进（从索引1到20），每个位置的内容移动到前一个位置（向入口方向移动）
    // 这样索引1的内容会移动到索引0，索引2的内容会移动到索引1，以此类推
    for (int i = 1; i <= 20; ++i) {
        if (i < m_emptyTraySlots.size() && i - 1 >= 0) {
            m_emptyTraySlots[i - 1] = m_emptyTraySlots[i];
            m_emptyTraySlots[i] = "";
        }
    }
    
    // 更新所有标签显示（标签索引1-21对应数组索引0-20）
    for (int i = 0; i < 21 && i < m_emptyTraySlots.size(); ++i) {
        int labelIndex = i + 1; // 标签索引 = 数组索引 + 1（跳过入口标签0）
        if (labelIndex < m_emptyTrayLabels.size() && m_emptyTrayLabels[labelIndex]) {
            m_emptyTrayLabels[labelIndex]->setText(m_emptyTraySlots[i]);
        }
    }
    
    // 确保出口标签始终保持初始文本，不显示车型
    if (22 < m_emptyTrayLabels.size() && m_emptyTrayLabels[22]) {
        m_emptyTrayLabels[22]->setText(m_emptyTrayExitLabelText);
    }
}

/**
 * @brief 保存空托盘可视化记录到数据库
 */
void tcpClient::saveEmptyTrayVisualizationRecords()
{
    // 检查数据库是否已打开
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "数据库未打开，跳过保存空托盘可视化记录";
        return;
    }
    
    QSqlQuery query;
    
    // 先清空旧记录
    if (!query.exec("DELETE FROM empty_tray_visualization_records")) {
        appendToLog("清空空托盘可视化记录表失败: " + query.lastError().text(), true);
        return;
    }
    
    // 插入当前所有槽位的记录（保存位置1-21，21个槽位，数组索引0-20）
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    query.prepare("INSERT INTO empty_tray_visualization_records (slot_position, vehicle_name, update_time) VALUES (?, ?, ?)");
    
    for (int i = 0; i < 21 && i < m_emptyTraySlots.size(); ++i) {
        if (!m_emptyTraySlots[i].isEmpty()) {
            query.addBindValue(i + 1); // 槽位位置（1-21）
            query.addBindValue(m_emptyTraySlots[i]);
            query.addBindValue(currentTime);
            if (!query.exec()) {
                appendToLog(QString("保存空托盘可视化记录失败（槽位%1）: %2").arg(i + 1).arg(query.lastError().text()), true);
            }
        }
    }
    
    qDebug() << "空托盘可视化记录已保存到数据库";
}

/**
 * @brief 从数据库加载空托盘可视化记录
 */
void tcpClient::loadEmptyTrayVisualizationRecords()
{
    // 检查数据库是否已打开
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "数据库未打开，跳过加载空托盘可视化记录";
        return;
    }
    
    // 确保数组已初始化
    if (m_emptyTraySlots.size() != 21) {
        m_emptyTraySlots.resize(21);
        for (int i = 0; i < 21; ++i) {
            m_emptyTraySlots[i] = "";
        }
    }
    
    // 先清空当前数组
    for (int i = 0; i < m_emptyTraySlots.size(); ++i) {
        m_emptyTraySlots[i] = "";
    }
    
    QSqlQuery query("SELECT slot_position, vehicle_name FROM empty_tray_visualization_records ORDER BY slot_position");
    
    if (query.lastError().isValid()) {
        qDebug() << "查询空托盘可视化记录失败:" << query.lastError().text();
        return;
    }
    
    while (query.next()) {
        int slotPosition = query.value(0).toInt();
        QString vehicleName = query.value(1).toString();
        
        // 槽位位置是1-21，数组索引是0-20（槽位位置 = 数组索引 + 1）
        if (slotPosition >= 1 && slotPosition <= 21) {
            int arrayIndex = slotPosition - 1;
            if (arrayIndex < m_emptyTraySlots.size()) {
                m_emptyTraySlots[arrayIndex] = vehicleName;
            }
        }
    }
    
    // 更新标签显示（标签索引1-21对应数组索引0-20）
    for (int i = 0; i < 21 && i < m_emptyTraySlots.size(); ++i) {
        int labelIndex = i + 1; // 标签索引 = 数组索引 + 1（跳过入口标签0）
        if (labelIndex < m_emptyTrayLabels.size() && m_emptyTrayLabels[labelIndex]) {
            m_emptyTrayLabels[labelIndex]->setText(m_emptyTraySlots[i]);
        }
    }
    
    // 确保出口标签始终保持初始文本，不显示车型
    if (22 < m_emptyTrayLabels.size() && m_emptyTrayLabels[22]) {
        m_emptyTrayLabels[22]->setText(m_emptyTrayExitLabelText);
    }
    
    qDebug() << "空托盘可视化记录已从数据库加载";
}

/**
 * @brief 保存统计信息到数据库
 */
void tcpClient::saveStatisticsInfo()
{
    // 检查数据库是否已打开
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "数据库未打开，跳过保存统计信息";
        return;
    }
    
    QSqlQuery query;
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    
    // 使用INSERT ... ON DUPLICATE KEY UPDATE来更新或插入
    // 先保存计划便次
    query.prepare("INSERT INTO statistics_info (stat_type, count_value, update_time) VALUES (?, ?, ?) "
                  "ON DUPLICATE KEY UPDATE count_value = ?, update_time = ?");
    query.addBindValue("planned_count");
    query.addBindValue(m_plannedCount);
    query.addBindValue(currentTime);
    query.addBindValue(m_plannedCount);
    query.addBindValue(currentTime);
    if (!query.exec()) {
        appendToLog("保存计划便次失败: " + query.lastError().text(), true);
        qWarning() << "保存计划便次失败:" << query.lastError().text();
    }
    
    // 保存实际便次
    query.prepare("INSERT INTO statistics_info (stat_type, count_value, update_time) VALUES (?, ?, ?) "
                  "ON DUPLICATE KEY UPDATE count_value = ?, update_time = ?");
    query.addBindValue("actual_count");
    query.addBindValue(m_actualCount);
    query.addBindValue(currentTime);
    query.addBindValue(m_actualCount);
    query.addBindValue(currentTime);
    if (!query.exec()) {
        appendToLog("保存实际便次失败: " + query.lastError().text(), true);
        qWarning() << "保存实际便次失败:" << query.lastError().text();
    }
    
    // 保存延迟便次
    query.prepare("INSERT INTO statistics_info (stat_type, count_value, update_time) VALUES (?, ?, ?) "
                  "ON DUPLICATE KEY UPDATE count_value = ?, update_time = ?");
    query.addBindValue("delayed_count");
    query.addBindValue(m_delayedCount);
    query.addBindValue(currentTime);
    query.addBindValue(m_delayedCount);
    query.addBindValue(currentTime);
    if (!query.exec()) {
        appendToLog("保存延迟便次失败: " + query.lastError().text(), true);
        qWarning() << "保存延迟便次失败:" << query.lastError().text();
    }
    
    qDebug() << "统计信息已保存到数据库";
}

/**
 * @brief 从数据库加载统计信息
 */
void tcpClient::loadStatisticsInfo()
{
    // 检查数据库是否已打开
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "数据库未打开，跳过加载统计信息";
        return;
    }
    
    // 检查标签是否已创建
    if (!plannedCountLabel || !actualCountLabel || !delayedCountLabel) {
        qDebug() << "统计信息标签未创建，跳过加载统计信息";
        return;
    }
    
    QSqlQuery query("SELECT stat_type, count_value FROM statistics_info");
    
    if (query.lastError().isValid()) {
        qDebug() << "查询统计信息失败:" << query.lastError().text();
        return;
    }
    
    while (query.next()) {
        QString statType = query.value(0).toString();
        int countValue = query.value(1).toInt();
        
        if (statType == "planned_count") {
            m_plannedCount = countValue;
            m_displayedPlannedCount = countValue;
            if (m_currentDisplayShift == "current" && plannedCountLabel) {
                plannedCountLabel->setText(QString("计划便次：%1便").arg(m_plannedCount));
            }
            qDebug() << "加载计划便次:" << m_plannedCount;
        } else if (statType == "actual_count") {
            m_actualCount = countValue;
            m_displayedActualCount = countValue;
            if (m_currentDisplayShift == "current" && actualCountLabel) {
                actualCountLabel->setText(QString("实际便次：%1便").arg(m_actualCount));
            }
            qDebug() << "加载实际便次:" << m_actualCount;
        } else if (statType == "delayed_count") {
            m_delayedCount = countValue;
            m_displayedDelayedCount = countValue;
            if (m_currentDisplayShift == "current" && delayedCountLabel) {
                delayedCountLabel->setText(QString("延迟便次：%1便").arg(m_delayedCount));
            }
            qDebug() << "加载延迟便次:" << m_delayedCount;
        }
    }
    
    // 更新班次显示按钮
    if (shiftDisplayButton) {
        updateShiftDisplayButton();
    }
    
    qDebug() << "统计信息已从数据库加载";
}

/**
 * @brief 插入异常记录到数据库
 */
void tcpClient::insertExceptionRecord(int slotNo, const QString &modelName, const QString &status, const QString &exceptionInfo, const QString &date)
{
    // 检查数据库是否已打开
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "数据库未打开，跳过插入异常记录";
        return;
    }
    
    QSqlQuery query;
    query.prepare("INSERT INTO exception_records (slot_no, model_name, status, exception_info, date) VALUES (?, ?, ?, ?, ?)");
    query.addBindValue(slotNo);
    query.addBindValue(modelName);
    query.addBindValue(status);
    query.addBindValue(exceptionInfo);
    query.addBindValue(date);
    if (!query.exec()) {
        appendToLog("插入异常记录失败: " + query.lastError().text(), true);
        qWarning() << "插入异常记录失败:" << query.lastError().text();
    } else {
        qDebug() << QString("异常记录已插入: 滑槽号=%1, 车型=%2, 状态=%3, 异常信息=%4, 日期=%5")
                    .arg(slotNo).arg(modelName).arg(status).arg(exceptionInfo).arg(date);
        
        // 同时更新表格显示（仅当该记录属于当前班次时才显示）
        if (exceptionTableWidget) {
            // 解析记录时间
            QDateTime recordDateTime = QDateTime::fromString(date, "yyyy-MM-dd HH:mm:ss");
            if (recordDateTime.isValid()) {
                // 如果班次设置未加载，先加载
                if (!m_shiftConfigLoaded) {
                    loadShiftConfig();
                }
                
                QString currentShift = getCurrentShift();
                QDateTime now = QDateTime::currentDateTime();
                QDate today = now.date();
                
                // 计算当前班次的时间范围（使用数据库中的班次设置）
                QDateTime shiftStartTime, shiftEndTime;
                if (currentShift == "白班") {
                    // 白班：当天 dayShiftStart - dayShiftEnd
                    shiftStartTime = QDateTime(today, m_dayShiftStart);
                    shiftEndTime = QDateTime(today, m_dayShiftEnd);
                } else {
                    // 夜班：当天 nightShiftStart - 次日 nightShiftEnd（需要加载当天和隔天的数据）
                    shiftStartTime = QDateTime(today, m_nightShiftStart);
                    shiftEndTime = QDateTime(today.addDays(1), m_nightShiftEnd);
                }
                
                // 判断记录是否在当前班次的时间范围内
                bool isInShift = false;
                if (currentShift == "白班") {
                    // 白班：当天 dayShiftStart - dayShiftEnd
                    isInShift = (recordDateTime.date() == today && 
                                recordDateTime >= shiftStartTime && 
                                recordDateTime < shiftEndTime);
                } else {
                    // 夜班：当天 nightShiftStart - 次日 nightShiftEnd（包括当天和隔天的数据）
                    isInShift = (recordDateTime >= shiftStartTime && recordDateTime < shiftEndTime);
                }
                
                // 只显示当前班次的记录
                if (isInShift) {
                    exceptionTableWidget->insertRow(0);
                    
                    // 滑槽号
                    QTableWidgetItem* item0 = new QTableWidgetItem(QString::number(slotNo));
                    item0->setTextAlignment(Qt::AlignCenter);
                    exceptionTableWidget->setItem(0, 0, item0);
                    
                    // 车型名称
                    QTableWidgetItem* item1 = new QTableWidgetItem(modelName);
                    item1->setTextAlignment(Qt::AlignCenter);
                    exceptionTableWidget->setItem(0, 1, item1);
                    
                    // 送入送出状态
                    QTableWidgetItem* item2 = new QTableWidgetItem(status);
                    item2->setTextAlignment(Qt::AlignCenter);
                    exceptionTableWidget->setItem(0, 2, item2);
                    
                    // 异常信息
                    QTableWidgetItem* item3 = new QTableWidgetItem(exceptionInfo);
                    item3->setTextAlignment(Qt::AlignCenter);
                    exceptionTableWidget->setItem(0, 3, item3);
                    
                    // 日期
                    QTableWidgetItem* item4 = new QTableWidgetItem(date);
                    item4->setTextAlignment(Qt::AlignCenter);
                    exceptionTableWidget->setItem(0, 4, item4);
                }
            }
        }
    }
}

/**
 * @brief 从数据库加载异常记录（仅加载当前班次的记录）
 */
void tcpClient::loadExceptionRecords()
{
    // 检查数据库是否已打开
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "数据库未打开，跳过加载异常记录";
        return;
    }
    
    // 检查表格是否已创建
    if (!exceptionTableWidget) {
        qDebug() << "异常表格未创建，跳过加载异常记录";
        return;
    }
    
    // 清空表格
    exceptionTableWidget->setRowCount(0);
    
    // 确保班次设置已加载（如果未加载，先加载）
    if (!m_shiftConfigLoaded) {
        loadShiftConfig();
    }
    
    // 获取当前班次（使用最新的班次设置）
    QString currentShift = getCurrentShift();
    QDateTime now = QDateTime::currentDateTime();
    QDate today = now.date();
    
    // 计算当前班次的时间范围（使用数据库中的班次设置）
    QDateTime shiftStartTime, shiftEndTime;
    if (currentShift == "白班") {
        // 白班：当天 dayShiftStart - dayShiftEnd
        shiftStartTime = QDateTime(today, m_dayShiftStart);
        shiftEndTime = QDateTime(today, m_dayShiftEnd);
    } else {
        // 夜班：当天 nightShiftStart - 次日 nightShiftEnd（需要加载当天和隔天的数据）
        shiftStartTime = QDateTime(today, m_nightShiftStart);
        shiftEndTime = QDateTime(today.addDays(1), m_nightShiftEnd);
    }
    
    qDebug() << QString("loadExceptionRecords: 当前班次=%1, 时间范围=%2 到 %3")
                .arg(currentShift)
                .arg(shiftStartTime.toString("yyyy-MM-dd HH:mm:ss"))
                .arg(shiftEndTime.toString("yyyy-MM-dd HH:mm:ss"));
    
    // 查询所有异常记录
    QSqlQuery query("SELECT slot_no, model_name, status, exception_info, date FROM exception_records ORDER BY id DESC");
    
    if (query.lastError().isValid()) {
        qDebug() << "查询异常记录失败:" << query.lastError().text();
        return;
    }
    
    int row = 0;
    while (query.next()) {
        int slotNo = query.value(0).toInt();
        QString modelName = query.value(1).toString();
        QString status = query.value(2).toString();
        QString exceptionInfo = query.value(3).toString();
        QString dateStr = query.value(4).toString();
        
        // 解析记录时间
        QDateTime recordDateTime = QDateTime::fromString(dateStr, "yyyy-MM-dd HH:mm:ss");
        if (!recordDateTime.isValid()) {
            // 如果日期格式不正确，跳过该记录
            continue;
        }
        
        // 判断记录是否在当前班次的时间范围内
        bool isInShift = false;
        if (currentShift == "白班") {
            // 白班：当天 7:15 - 17:30
            isInShift = (recordDateTime.date() == today && 
                        recordDateTime >= shiftStartTime && 
                        recordDateTime < shiftEndTime);
        } else {
            // 夜班：当天 17:30 - 次日 7:15（包括当天和隔天的数据）
            isInShift = (recordDateTime >= shiftStartTime && recordDateTime < shiftEndTime);
        }
        
        // 只显示当前班次的记录
        if (!isInShift) {
            continue;
        }
        
        exceptionTableWidget->insertRow(row);
        
        // 滑槽号
        QTableWidgetItem* item0 = new QTableWidgetItem(QString::number(slotNo));
        item0->setTextAlignment(Qt::AlignCenter);
        exceptionTableWidget->setItem(row, 0, item0);
        
        // 车型名称
        QTableWidgetItem* item1 = new QTableWidgetItem(modelName);
        item1->setTextAlignment(Qt::AlignCenter);
        exceptionTableWidget->setItem(row, 1, item1);
        
        // 送入送出状态
        QTableWidgetItem* item2 = new QTableWidgetItem(status);
        item2->setTextAlignment(Qt::AlignCenter);
        exceptionTableWidget->setItem(row, 2, item2);
        
        // 异常信息
        QTableWidgetItem* item3 = new QTableWidgetItem(exceptionInfo);
        item3->setTextAlignment(Qt::AlignCenter);
        exceptionTableWidget->setItem(row, 3, item3);
        
        // 日期
        QTableWidgetItem* item4 = new QTableWidgetItem(dateStr);
        item4->setTextAlignment(Qt::AlignCenter);
        exceptionTableWidget->setItem(row, 4, item4);
        
        row++;
    }
    
    qDebug() << QString("异常记录已从数据库加载（当前班次：%1），共%2条").arg(currentShift).arg(row);
}

/**
 * @brief 初始化班次记录表
 */
void tcpClient::initShiftTable()
{
    // 检查数据库是否已打开
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "数据库未打开，跳过初始化班次记录表";
        return;
    }
    
    QSqlQuery query;
    // 班次记录表
    if (query.exec("CREATE TABLE IF NOT EXISTS shift_records ("
                   "id INT PRIMARY KEY AUTO_INCREMENT,"
                   "shift_type VARCHAR(20),"
                   "actual_count INT,"
                   "shift_date DATE,"
                   "shift_time TIME,"
                   "create_time DATETIME)")) {
        qDebug() << "班次记录表创建/检查成功";
    } else {
        qWarning() << "班次记录表创建失败:" << query.lastError().text();
    }
}

/**
 * @brief 检查班次变化
 * 根据数据库中的班次设置，在班次切换时间保存记录并清零
 * 班次切换时会自动重新加载当前班次表格
 */
void tcpClient::checkShiftChange()
{
    // 如果班次设置未加载，先加载
    if (!m_shiftConfigLoaded) {
        loadShiftConfig();
    }
    
    QDateTime now = QDateTime::currentDateTime();
    QTime currentTime = now.time();
    QDate currentDate = now.date();
    
    // 使用从数据库读取的班次设置
    QTime dayShiftStart = m_dayShiftStart;
    QTime nightShiftStart = m_nightShiftStart;
    
    // 检查是否是白班开始时间
    if (currentTime.hour() == dayShiftStart.hour() && currentTime.minute() == dayShiftStart.minute()) {
        // 保存当前实际便次到数据库（夜班记录）
        saveShiftRecord("夜班");
        
        // 清零实际便次，开始记录白班数据
        m_actualCount = 0;
        if (m_currentDisplayShift == "current" && actualCountLabel) {
            actualCountLabel->setText(QString("实际便次：%1便").arg(m_actualCount));
        }
        m_displayedActualCount = m_actualCount; // 更新显示值
        saveStatisticsInfo(); // 保存清零后的实际便次
        
        // 重新加载当前班次表格（从数据表格加载当前班次数据）
        loadCurrentShiftRecordsFromDb();
        appendToLog(QString("班次切换：已重新加载当前班次表格（白班数据）"), false);
        
        appendToLog(QString("班次切换：夜班结束，实际便次已保存。白班开始，实际便次已清零"), false);
        
        // 更新工程组统计的班次按钮
        if (projectGroupShiftButton) {
            projectGroupShiftButton->setText("白班");
        }
        // 如果当前正在显示工程组记录页面，更新统计表格
        if (ui->stackedWidget->currentIndex() == 3) {
            updateProjectGroupStatistics();
        }
        
        // 重新加载异常记录（显示新班次的记录）
        loadExceptionRecords();
    }
    // 检查是否是夜班开始时间
    else if (currentTime.hour() == nightShiftStart.hour() && currentTime.minute() == nightShiftStart.minute()) {
        // 保存当前实际便次到数据库（白班记录）
        saveShiftRecord("白班");
        
        // 清零实际便次，开始记录夜班数据
        m_actualCount = 0;
        if (m_currentDisplayShift == "current" && actualCountLabel) {
            actualCountLabel->setText(QString("实际便次：%1便").arg(m_actualCount));
        }
        m_displayedActualCount = m_actualCount; // 更新显示值
        saveStatisticsInfo(); // 保存清零后的实际便次
        
        // 重新加载当前班次表格（从数据表格加载当前班次数据）
        loadCurrentShiftRecordsFromDb();
        appendToLog(QString("班次切换：已重新加载当前班次表格（夜班数据）"), false);
        
        appendToLog(QString("班次切换：白班结束，实际便次已保存。夜班开始，实际便次已清零"), false);
        
        // 更新工程组统计的班次按钮
        if (projectGroupShiftButton) {
            projectGroupShiftButton->setText("夜班");
        }
        // 如果当前正在显示工程组记录页面，更新统计表格
        if (ui->stackedWidget->currentIndex() == 3) {
            updateProjectGroupStatistics();
        }
        
        // 重新加载异常记录（显示新班次的记录）
        loadExceptionRecords();
    }
}

/**
 * @brief 保存班次记录到数据库
 * @param shiftType 班次类型（"白班"或"夜班"）
 */
void tcpClient::saveShiftRecord(const QString &shiftType)
{
    // 检查数据库是否已打开
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "数据库未打开，跳过保存班次记录";
        return;
    }
    
    QDateTime now = QDateTime::currentDateTime();
    QDate currentDate = now.date();
    QTime currentTime = now.time();
    
    // 如果是保存夜班记录，且当前时间是7:15，日期应该是昨天
    if (shiftType == "夜班" && currentTime.hour() == 7 && currentTime.minute() == 15) {
        currentDate = currentDate.addDays(-1);
    }
    
    QSqlQuery query;
    query.prepare("INSERT INTO shift_records (shift_type, actual_count, shift_date, shift_time, create_time) "
                  "VALUES (?, ?, ?, ?, ?)");
    query.addBindValue(shiftType);
    query.addBindValue(m_actualCount);
    query.addBindValue(currentDate.toString("yyyy-MM-dd"));
    query.addBindValue(currentTime.toString("hh:mm:ss"));
    query.addBindValue(now.toString("yyyy-MM-dd hh:mm:ss"));
    
    if (!query.exec()) {
        appendToLog(QString("保存%1记录失败: %2").arg(shiftType).arg(query.lastError().text()), true);
        qWarning() << "保存班次记录失败:" << query.lastError().text();
    } else {
        appendToLog(QString("已保存%1记录：实际便次=%2便，日期=%3，时间=%4")
                   .arg(shiftType)
                   .arg(m_actualCount)
                   .arg(currentDate.toString("yyyy-MM-dd"))
                   .arg(currentTime.toString("hh:mm:ss")), false);
        qDebug() << QString("班次记录已保存：%1，实际便次=%2").arg(shiftType).arg(m_actualCount);
    }
}

/**
 * @brief 保存ED软件连接配置按钮点击处理
 */
void tcpClient::onSaveEdSoftwareConnectionConfigClicked()
{
    if (!lineEditEdSoftwareIP || !lineEditEdSoftwarePort) {
        QMessageBox::warning(this, "错误", "ED软件连接设置未初始化！");
        return;
    }
    
    QString ip = lineEditEdSoftwareIP->text().trimmed();
    QString portStr = lineEditEdSoftwarePort->text().trimmed();
    
    // 验证输入
    if (ip.isEmpty()) {
        QMessageBox::warning(this, "配置错误", "ED软件IP地址不能为空！");
        return;
    }
    
    bool ok;
    int port = portStr.toInt(&ok);
    if (!ok || port <= 0 || port > 65535) {
        QMessageBox::warning(this, "配置错误", "请输入有效的端口号(1-65535)！");
        return;
    }
    
    saveConnectionConfig("ed_software", ip, port);
    QMessageBox::information(this, "保存成功", QString("ED软件连接配置已保存！\n\nIP: %1\n端口: %2").arg(ip).arg(port));
}

/**
 * @brief 初始化班次设置表
 */
void tcpClient::initShiftConfigTable()
{
    // 检查数据库是否已打开
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "数据库未打开，跳过初始化班次设置表";
        return;
    }
    
    QSqlQuery query;
    // 班次设置表
    if (query.exec("CREATE TABLE IF NOT EXISTS shift_config ("
                   "id INT PRIMARY KEY AUTO_INCREMENT,"
                   "shift_type VARCHAR(20) UNIQUE,"
                   "start_time TIME,"
                   "end_time TIME,"
                   "update_time VARCHAR(255))")) {
        qDebug() << "班次设置表创建/检查成功";
    } else {
        qWarning() << "班次设置表创建失败:" << query.lastError().text();
    }
}

/**
 * @brief 保存班次设置到数据库
 */
void tcpClient::saveShiftConfig()
{
    // 检查数据库是否已打开
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        QMessageBox::warning(this, "错误", "数据库未连接，无法保存班次设置！");
        return;
    }
    
    if (!timeEditDayShiftStart || !timeEditDayShiftEnd || 
        !timeEditNightShiftStart || !timeEditNightShiftEnd) {
        QMessageBox::warning(this, "错误", "班次设置控件未初始化！");
        return;
    }
    
    QTime dayShiftStart = timeEditDayShiftStart->time();
    QTime dayShiftEnd = timeEditDayShiftEnd->time();
    QTime nightShiftStart = timeEditNightShiftStart->time();
    QTime nightShiftEnd = timeEditNightShiftEnd->time();
    
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QSqlQuery query;
    
    // 保存白班设置
    query.prepare("INSERT INTO shift_config (shift_type, start_time, end_time, update_time) VALUES (?, ?, ?, ?) "
                  "ON DUPLICATE KEY UPDATE start_time = ?, end_time = ?, update_time = ?");
    query.addBindValue("白班");
    query.addBindValue(dayShiftStart.toString("HH:mm:ss"));
    query.addBindValue(dayShiftEnd.toString("HH:mm:ss"));
    query.addBindValue(currentTime);
    query.addBindValue(dayShiftStart.toString("HH:mm:ss"));
    query.addBindValue(dayShiftEnd.toString("HH:mm:ss"));
    query.addBindValue(currentTime);
    
    if (!query.exec()) {
        appendToLog("保存白班设置失败: " + query.lastError().text(), true);
        QMessageBox::warning(this, "保存失败", "保存白班设置失败: " + query.lastError().text());
        return;
    }
    
    // 保存夜班设置
    query.prepare("INSERT INTO shift_config (shift_type, start_time, end_time, update_time) VALUES (?, ?, ?, ?) "
                  "ON DUPLICATE KEY UPDATE start_time = ?, end_time = ?, update_time = ?");
    query.addBindValue("夜班");
    query.addBindValue(nightShiftStart.toString("HH:mm:ss"));
    query.addBindValue(nightShiftEnd.toString("HH:mm:ss"));
    query.addBindValue(currentTime);
    query.addBindValue(nightShiftStart.toString("HH:mm:ss"));
    query.addBindValue(nightShiftEnd.toString("HH:mm:ss"));
    query.addBindValue(currentTime);
    
    if (!query.exec()) {
        appendToLog("保存夜班设置失败: " + query.lastError().text(), true);
        QMessageBox::warning(this, "保存失败", "保存夜班设置失败: " + query.lastError().text());
        return;
    }
    
    // 更新成员变量
    m_dayShiftStart = dayShiftStart;
    m_dayShiftEnd = dayShiftEnd;
    m_nightShiftStart = nightShiftStart;
    m_nightShiftEnd = nightShiftEnd;
    m_shiftConfigLoaded = true;
    
    appendToLog(QString("班次设置已保存: 白班 %1-%2, 夜班 %3-%4")
                .arg(dayShiftStart.toString("HH:mm"))
                .arg(dayShiftEnd.toString("HH:mm"))
                .arg(nightShiftStart.toString("HH:mm"))
                .arg(nightShiftEnd.toString("HH:mm")));
    
    // 确保使用最新的班次设置刷新相关表格
    // 注意：成员变量已经更新，getCurrentShift() 会使用新的班次设置
    QString currentShift = getCurrentShift();
    qDebug() << QString("班次设置已更新，当前班次: %1, 开始刷新表格").arg(currentShift);
    qDebug() << QString("白班: %1-%2, 夜班: %3-%4")
                .arg(m_dayShiftStart.toString("HH:mm"))
                .arg(m_dayShiftEnd.toString("HH:mm"))
                .arg(m_nightShiftStart.toString("HH:mm"))
                .arg(m_nightShiftEnd.toString("HH:mm"));
    
    // 刷新相关表格（根据新的班次设置重新加载数据）
    loadCurrentShiftRecordsFromDb();  // 刷新当前班次表格
    loadExceptionRecords();           // 刷新异常记录表格
    updateProjectGroupStatistics();   // 刷新工程组记录表格
    
    appendToLog(QString("表格已根据新的班次设置刷新（当前班次: %1）").arg(currentShift), false);
    
    QMessageBox::information(this, "保存成功", 
                            QString("班次设置已保存！\n\n"
                                   "白班: %1 - %2\n"
                                   "夜班: %3 - %4\n\n"
                                   "相关表格已自动刷新")
                            .arg(dayShiftStart.toString("HH:mm"))
                            .arg(dayShiftEnd.toString("HH:mm"))
                            .arg(nightShiftStart.toString("HH:mm"))
                            .arg(nightShiftEnd.toString("HH:mm")));
}

/**
 * @brief 从数据库加载班次设置
 */
void tcpClient::loadShiftConfig()
{
    // 检查数据库是否已打开
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "数据库未打开，跳过加载班次设置";
        return;
    }
    
    if (!timeEditDayShiftStart || !timeEditDayShiftEnd || 
        !timeEditNightShiftStart || !timeEditNightShiftEnd) {
        qDebug() << "班次设置控件未初始化，跳过加载班次设置";
        return;
    }
    
    QSqlQuery query("SELECT shift_type, start_time, end_time FROM shift_config");
    
    if (query.lastError().isValid()) {
        qDebug() << "查询班次设置失败:" << query.lastError().text();
        return;
    }
    
    while (query.next()) {
        QString shiftType = query.value(0).toString();
        QString startTimeStr = query.value(1).toString();
        QString endTimeStr = query.value(2).toString();
        
        QTime startTime = QTime::fromString(startTimeStr, "HH:mm:ss");
        QTime endTime = QTime::fromString(endTimeStr, "HH:mm:ss");
        
        if (shiftType == "白班") {
            if (startTime.isValid()) {
                m_dayShiftStart = startTime;
                if (timeEditDayShiftStart) {
                    timeEditDayShiftStart->setTime(startTime);
                }
            }
            if (endTime.isValid()) {
                m_dayShiftEnd = endTime;
                if (timeEditDayShiftEnd) {
                    timeEditDayShiftEnd->setTime(endTime);
                }
            }
        } else if (shiftType == "夜班") {
            if (startTime.isValid()) {
                m_nightShiftStart = startTime;
                if (timeEditNightShiftStart) {
                    timeEditNightShiftStart->setTime(startTime);
                }
            }
            if (endTime.isValid()) {
                m_nightShiftEnd = endTime;
                if (timeEditNightShiftEnd) {
                    timeEditNightShiftEnd->setTime(endTime);
                }
            }
        }
    }
    
    m_shiftConfigLoaded = true;
    qDebug() << QString("班次设置已从数据库加载: 白班 %1-%2, 夜班 %3-%4")
                .arg(m_dayShiftStart.toString("HH:mm"))
                .arg(m_dayShiftEnd.toString("HH:mm"))
                .arg(m_nightShiftStart.toString("HH:mm"))
                .arg(m_nightShiftEnd.toString("HH:mm"));
}

/**
 * @brief 保存班次设置按钮点击处理
 */
void tcpClient::onSaveShiftConfigClicked()
{
    saveShiftConfig();
}

/**
 * @brief ED软件连接按钮点击处理
 */
void tcpClient::onEdSoftwareConnectClicked()
{
    if (!lineEditEdSoftwareIP || !lineEditEdSoftwarePort) {
        QMessageBox::warning(this, "错误", "ED软件连接设置未初始化！");
        return;
    }
    
    QString edSoftwareIP = lineEditEdSoftwareIP->text().trimmed();
    QString portStr = lineEditEdSoftwarePort->text().trimmed();

    qInfo() << "用户尝试连接ED软件:" << edSoftwareIP << ":" << portStr;

    // 验证IP地址
    if (edSoftwareIP.isEmpty()) {
        QString errorMsg = "请输入ED软件IP地址";
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
    QString connectMsg = QString("正在连接到ED软件 %1:%2...").arg(edSoftwareIP).arg(port);
    appendToLog(connectMsg);
    qInfo() << connectMsg;

    m_edSoftwareSocket->connectToHost(edSoftwareIP, port);
    m_edSoftwareConnectionTimer->start();

    pushButtonEdSoftwareConnect->setEnabled(false);
}

/**
 * @brief ED软件断开按钮点击处理
 */
void tcpClient::onEdSoftwareDisconnectClicked()
{
    if (m_edSoftwareSocket->state() == QAbstractSocket::ConnectedState) {
        m_edSoftwareSocket->disconnectFromHost();
        appendToLog("正在断开ED软件连接...");
    }
}

/**
 * @brief ED软件Socket连接成功处理
 */
void tcpClient::onEdSoftwareSocketConnected()
{
    m_edSoftwareConnectionTimer->stop();
    m_isEdSoftwareConnected = true;
    updateEdSoftwareConnectionStatus(true);
    
    QString successMsg = "ED软件连接成功！";
    appendToLog(successMsg, false);
    qInfo() << successMsg << "ED软件地址:" << m_edSoftwareSocket->peerAddress().toString() << "端口:" << m_edSoftwareSocket->peerPort();
}

/**
 * @brief ED软件Socket断开连接处理
 */
void tcpClient::onEdSoftwareSocketDisconnected()
{
    m_edSoftwareConnectionTimer->stop();
    m_isEdSoftwareConnected = false;
    updateEdSoftwareConnectionStatus(false);
    
    appendToLog("ED软件连接已断开", false);
}

/**
 * @brief ED软件Socket错误处理
 * @param error 错误类型
 */
void tcpClient::onEdSoftwareSocketError(QAbstractSocket::SocketError error)
{
    m_edSoftwareConnectionTimer->stop();
    m_isEdSoftwareConnected = false;
    updateEdSoftwareConnectionStatus(false);

    QString errorMsg = m_edSoftwareSocket->errorString();
    appendToLog(QString("ED软件连接错误: %1").arg(errorMsg), true);
}

/**
 * @brief ED软件Socket数据可读处理
 */
void tcpClient::onEdSoftwareSocketReadyRead()
{
    QByteArray data = m_edSoftwareSocket->readAll();
    appendToLog(QString("收到ED软件数据: %1 字节").arg(data.size()), false);
    qDebug() << "ED软件数据:" << data;
    
    // 处理接收到的JSON数据（不保存到表格）
    processServerJsonData(data, false);
}

/**
 * @brief ED软件连接超时处理
 */
void tcpClient::onEdSoftwareConnectionTimeout()
{
    m_edSoftwareSocket->abort();
    appendToLog("ED软件连接超时", true);
    updateEdSoftwareConnectionStatus(false);
}

/**
 * @brief 更新ED软件连接状态显示
 * @param connected 连接状态
 */
void tcpClient::updateEdSoftwareConnectionStatus(bool connected)
{
    if (!labelEdSoftwareConnectionStatus || !pushButtonEdSoftwareConnect || !pushButtonEdSoftwareDisconnect) {
        return;
    }
    
    if (connected) {
        labelEdSoftwareConnectionStatus->setText("已连接");
        pushButtonEdSoftwareConnect->setEnabled(false);
        pushButtonEdSoftwareDisconnect->setEnabled(true);
        if (lineEditEdSoftwareIP) lineEditEdSoftwareIP->setEnabled(false);
        if (lineEditEdSoftwarePort) lineEditEdSoftwarePort->setEnabled(false);
    } else {
        labelEdSoftwareConnectionStatus->setText("未连接");
        pushButtonEdSoftwareConnect->setEnabled(true);
        pushButtonEdSoftwareDisconnect->setEnabled(false);
        if (lineEditEdSoftwareIP) lineEditEdSoftwareIP->setEnabled(true);
        if (lineEditEdSoftwarePort) lineEditEdSoftwarePort->setEnabled(true);
    }
    
    // 更新右下角状态标签
    if (m_labelEdSoftwareStatus) {
        if (connected) {
            m_labelEdSoftwareStatus->setText("ED软件: 已连接");
            m_labelEdSoftwareStatus->setStyleSheet("QLabel { color: green; font-weight: bold; }");
        } else {
            m_labelEdSoftwareStatus->setText("ED软件: 未连接");
            m_labelEdSoftwareStatus->setStyleSheet("QLabel { color: red; font-weight: bold; }");
        }
    }
}

/**
 * @brief 构建可视化数据JSON对象
 * @return JSON对象
 */
QJsonObject tcpClient::buildVisualizationData()
{
    QJsonObject root;
    root["action"] = "show";
    root["type"] = "AGT Transport";
    
    // 统计信息
    QJsonObject statistics;
    statistics["planned_count"] = m_plannedCount;
    statistics["actual_count"] = m_actualCount;
    statistics["delayed_count"] = m_delayedCount;
    root["statistics"] = statistics;
    
    // 实托盘槽位数组（21个槽位）
    QJsonArray realTraySlots;
    for (int i = 0; i < 21 && i < m_realTraySlots.size(); ++i) {
        realTraySlots.append(m_realTraySlots[i]);
    }
    // 如果数组不足21个，补充空字符串
    while (realTraySlots.size() < 21) {
        realTraySlots.append("");
    }
    root["real_tray_slots"] = realTraySlots;
    
    // 空托盘槽位数组（21个槽位）
    QJsonArray emptyTraySlots;
    for (int i = 0; i < 21 && i < m_emptyTraySlots.size(); ++i) {
        emptyTraySlots.append(m_emptyTraySlots[i]);
    }
    // 如果数组不足21个，补充空字符串
    while (emptyTraySlots.size() < 21) {
        emptyTraySlots.append("");
    }
    root["empty_tray_slots"] = emptyTraySlots;
    
    return root;
}

/**
 * @brief 发送可视化数据到服务端
 */
void tcpClient::sendVisualizationDataToServer()
{
    // 检查服务端连接状态
    if (!m_serverSocket || m_serverSocket->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "服务端未连接，跳过发送可视化数据";
        return; // 未连接，不发送
    }
    
    // 构建JSON数据
    QJsonObject jsonData = buildVisualizationData();
    QJsonDocument doc(jsonData);
    QByteArray jsonBytes = doc.toJson();
    
    // 发送数据
    qint64 bytesWritten = m_serverSocket->write(jsonBytes);
    if (bytesWritten > 0) {
        qDebug() << "已发送AGT搬运数据到服务端，大小:" << bytesWritten << "字节";
        // 成功时不写入日志，减少日志量
    } else {
        appendToLog("发送AGT搬运数据到服务端失败", true);
        qWarning() << "发送AGT搬运数据失败";
    }
    
    // 启动工程组数据发送定时器（1秒后发送）
    m_projectGroupDataTimer->start();
}

/**
 * @brief 构建工程组数据JSON对象
 * @return JSON对象
 */
QJsonObject tcpClient::buildProjectGroupData()
{
    QJsonObject root;
    root["action"] = "show";
    root["type"] = "Project Group Statistics";
    
    // 检查数据库是否已打开
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "数据库未打开，返回空的工程组数据";
        root["statistics"] = QJsonArray();
        return root;
    }
    
    // 获取所有车型名称（从车型绑定表）
    QSqlQuery query;
    query.prepare("SELECT DISTINCT model_name FROM model_bindings ORDER BY model_name");
    
    if (!query.exec()) {
        qWarning() << "查询车型列表失败:" << query.lastError().text();
        root["statistics"] = QJsonArray();
        return root;
    }
    
    QMap<QString, QMap<QString, int>> statistics; // 车型名称 -> {操作类型 -> 数量}
    
    // 初始化所有车型的统计数据
    while (query.next()) {
        QString modelName = query.value(0).toString();
        statistics[modelName]["实托盘搬入"] = 0;
        statistics[modelName]["实托盘搬出"] = 0;
        statistics[modelName]["空托盘搬入"] = 0;
        statistics[modelName]["空托盘搬出"] = 0;
    }
    
    // 根据显示的班次决定统计哪个班次的数据（上报时使用当前班次，不使用前一个班次）
    QString currentShift = getCurrentShift();
    QDateTime now = QDateTime::currentDateTime();
    
    // 计算当前班次的时间范围
    QDateTime shiftStartTime, shiftEndTime;
    if (currentShift == "白班") {
        // 白班：今天7:15 - 今天17:30
        shiftStartTime = QDateTime(now.date(), QTime(7, 15, 0));
        shiftEndTime = QDateTime(now.date(), QTime(17, 30, 0));
    } else {
        // 夜班：昨天17:30 - 今天7:15
        shiftStartTime = QDateTime(now.date().addDays(-1), QTime(17, 30, 0));
        shiftEndTime = QDateTime(now.date(), QTime(7, 15, 0));
    }
    
    // 从数据记录表统计各车型的操作次数
    query.prepare("SELECT model_name, status, time FROM data_records WHERE status IN ('实托盘搬入', '实托盘搬出', '空托盘搬入', '空托盘搬出')");
    
    if (!query.exec()) {
        qWarning() << "查询统计数据失败:" << query.lastError().text();
        root["statistics"] = QJsonArray();
        return root;
    }
    
    while (query.next()) {
        QString modelName = query.value(0).toString();
        QString status = query.value(1).toString();
        QString timeStr = query.value(2).toString();
        
        // 解析时间字符串（格式：yyyy-MM-dd hh:mm:ss 或 yyyy-MM-dd HH:mm:ss）
        QDateTime recordTime = QDateTime::fromString(timeStr, "yyyy-MM-dd HH:mm:ss");
        if (!recordTime.isValid()) {
            // 尝试12小时制格式
            recordTime = QDateTime::fromString(timeStr, "yyyy-MM-dd hh:mm:ss");
        }
        
        if (!recordTime.isValid()) {
            qWarning() << "无法解析时间字符串:" << timeStr;
            continue;
        }
        
        // 判断记录是否属于当前班次
        bool isInShift = false;
        if (currentShift == "白班") {
            // 白班：7:15 - 17:30（同一天）
            isInShift = (recordTime >= shiftStartTime && recordTime < shiftEndTime);
        } else {
            // 夜班：17:30 - 次日7:15（跨天）
            isInShift = (recordTime >= shiftStartTime && recordTime < shiftEndTime);
        }
        
        if (isInShift && statistics.contains(modelName)) {
            statistics[modelName][status]++;
        }
    }
    
    // 构建统计数组
    QJsonArray statisticsArray;
    QList<QString> modelNames = statistics.keys();
    std::sort(modelNames.begin(), modelNames.end());
    
    for (const QString& modelName : modelNames) {
        QJsonObject modelData;
        modelData["model_name"] = modelName;
        modelData["real_tray_in"] = statistics[modelName]["实托盘搬入"];
        modelData["real_tray_out"] = statistics[modelName]["实托盘搬出"];
        modelData["empty_tray_in"] = statistics[modelName]["空托盘搬入"];
        modelData["empty_tray_out"] = statistics[modelName]["空托盘搬出"];
        statisticsArray.append(modelData);
    }
    
    root["statistics"] = statisticsArray;
    root["shift"] = currentShift; // 添加当前班次信息
    
    return root;
}

/**
 * @brief 发送工程组数据到服务端
 */
void tcpClient::sendProjectGroupDataToServer()
{
    // 检查服务端连接状态
    if (!m_serverSocket || m_serverSocket->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "服务端未连接，跳过发送工程组数据";
        return; // 未连接，不发送
    }
    
    // 构建JSON数据
    QJsonObject jsonData = buildProjectGroupData();
    QJsonDocument doc(jsonData);
    QByteArray jsonBytes = doc.toJson();
    
    // 发送数据
    qint64 bytesWritten = m_serverSocket->write(jsonBytes);
    if (bytesWritten > 0) {
        qDebug() << "已发送工程组数据到服务端，大小:" << bytesWritten << "字节";
        // 成功时不写入日志，减少日志量
    } else {
        appendToLog("发送工程组数据到服务端失败", true);
        qWarning() << "发送工程组数据失败";
    }
    
    // 启动异常数据发送定时器（1秒后发送异常记录）
    m_exceptionDataTimer->start();
}

/**
 * @brief 构建异常记录数据JSON对象（当前班次）
 * @return JSON对象
 */
QJsonObject tcpClient::buildExceptionData()
{
    QJsonObject root;
    root["action"] = "show";
    root["type"] = "Exception Records";
    
    // 检查数据库是否已打开
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "数据库未打开，返回空的异常记录数据";
        root["exceptions"] = QJsonArray();
        root["shift"] = getCurrentShift();
        return root;
    }
    
    // 获取当前班次
    QString currentShift = getCurrentShift();
    QDateTime now = QDateTime::currentDateTime();
    
    // 计算当前班次的时间范围
    QDateTime shiftStartTime, shiftEndTime;
    if (currentShift == "白班") {
        // 白班：今天7:15 - 今天17:30
        shiftStartTime = QDateTime(now.date(), QTime(7, 15, 0));
        shiftEndTime = QDateTime(now.date(), QTime(17, 30, 0));
    } else {
        // 夜班：昨天17:30 - 今天7:15
        shiftStartTime = QDateTime(now.date().addDays(-1), QTime(17, 30, 0));
        shiftEndTime = QDateTime(now.date(), QTime(7, 15, 0));
    }
    
    // 查询当前班次的异常记录
    QSqlQuery query;
    query.prepare("SELECT slot_no, model_name, status, exception_info, date FROM exception_records ORDER BY id DESC");
    
    if (!query.exec()) {
        qWarning() << "查询异常记录失败:" << query.lastError().text();
        root["exceptions"] = QJsonArray();
        root["shift"] = currentShift;
        return root;
    }
    
    // 构建异常记录数组
    QJsonArray exceptionsArray;
    
    while (query.next()) {
        int slotNo = query.value(0).toInt();
        QString modelName = query.value(1).toString();
        QString status = query.value(2).toString();
        QString exceptionInfo = query.value(3).toString();
        QString dateStr = query.value(4).toString();
        
        // 解析时间字符串（格式：yyyy-MM-dd HH:mm:ss）
        QDateTime recordTime = QDateTime::fromString(dateStr, "yyyy-MM-dd HH:mm:ss");
        if (!recordTime.isValid()) {
            qWarning() << "无法解析异常记录时间字符串:" << dateStr;
            continue;
        }
        
        // 判断记录是否属于当前班次
        bool isInShift = false;
        if (currentShift == "白班") {
            // 白班：7:15 - 17:30（同一天）
            isInShift = (recordTime >= shiftStartTime && recordTime < shiftEndTime);
        } else {
            // 夜班：17:30 - 次日7:15（跨天）
            isInShift = (recordTime >= shiftStartTime && recordTime < shiftEndTime);
        }
        
        // 只包含当前班次的异常记录
        if (isInShift) {
            QJsonObject exceptionData;
            exceptionData["slot_no"] = slotNo;
            exceptionData["model_name"] = modelName;
            exceptionData["status"] = status;
            exceptionData["exception_info"] = exceptionInfo;
            exceptionData["date"] = dateStr;
            exceptionsArray.append(exceptionData);
        }
    }
    
    root["exceptions"] = exceptionsArray;
    root["shift"] = currentShift; // 添加当前班次信息
    
    return root;
}

/**
 * @brief 发送异常记录数据到服务端
 */
void tcpClient::sendExceptionDataToServer()
{
    // 检查服务端连接状态
    if (!m_serverSocket || m_serverSocket->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "服务端未连接，跳过发送异常记录数据";
        return; // 未连接，不发送
    }
    
    // 构建JSON数据
    QJsonObject jsonData = buildExceptionData();
    QJsonDocument doc(jsonData);
    QByteArray jsonBytes = doc.toJson();
    
    // 发送数据
    qint64 bytesWritten = m_serverSocket->write(jsonBytes);
    if (bytesWritten > 0) {
        qDebug() << "已发送异常记录数据到服务端，大小:" << bytesWritten << "字节";
        // 成功时不写入日志，减少日志量
    } else {
        appendToLog("发送异常记录数据到服务端失败", true);
        qWarning() << "发送异常记录数据失败";
    }
}

/**
 * @brief 获取当前班次（"白班"或"夜班"）
 * @return 当前班次字符串
 */
QString tcpClient::getCurrentShift()
{
    QDateTime now = QDateTime::currentDateTime();
    return getShiftFromDateTime(now);
}

/**
 * @brief 根据日期时间获取班次（"白班"或"夜班"）
 * @param dateTime 日期时间
 * @return 班次字符串（"白班"或"夜班"）
 */
QString tcpClient::getShiftFromDateTime(const QDateTime &dateTime)
{
    // 如果班次设置未加载，先加载
    if (!m_shiftConfigLoaded) {
        loadShiftConfig();
    }
    
    QTime currentTime = dateTime.time();
    QDate currentDate = dateTime.date();
    
    // 使用从数据库读取的班次设置
    QTime dayShiftStart = m_dayShiftStart;
    QTime dayShiftEnd = m_dayShiftEnd;
    QTime nightShiftStart = m_nightShiftStart;
    QTime nightShiftEnd = m_nightShiftEnd;
    
    // 判断当前时间属于哪个班次
    // 白班：dayShiftStart - dayShiftEnd（通常在同一天）
    // 夜班：nightShiftStart - 次日 nightShiftEnd（跨天）
    
    // 先判断是否在白班时间范围内
    bool isDayShift = false;
    if (dayShiftStart <= dayShiftEnd) {
        // 白班在同一天内（例如 7:15 - 17:30）
        isDayShift = (currentTime >= dayShiftStart && currentTime < dayShiftEnd);
    } else {
        // 白班跨天（例如 22:00 - 次日 6:00），这种情况较少见
        isDayShift = (currentTime >= dayShiftStart || currentTime < dayShiftEnd);
    }
    
    if (isDayShift) {
        return "白班";
    } else {
        return "夜班";
    }
}

/**
 * @brief 更新班次显示按钮文本
 */
void tcpClient::updateShiftDisplayButton()
{
    if (!shiftDisplayButton) {
        return;
    }
    
    QString currentShift = getCurrentShift();
    QString buttonText;
    
    if (m_currentDisplayShift == "current") {
        buttonText = QString("当前班次：%1").arg(currentShift);
        shiftDisplayButton->setStyleSheet(
            "QPushButton {"
            "font-size: 12pt;"
            "font-weight: bold;"
            "padding: 8px 16px;"
            "min-width: 100px;"
            "border: 2px solid #4CAF50;"
            "border-radius: 5px;"
            "background-color: #E8F5E9;"
            "}"
            "QPushButton:hover {"
            "background-color: #C8E6C9;"
            "}"
        );
    } else {
        QString previousShift = (currentShift == "白班") ? "夜班" : "白班";
        buttonText = QString("前一个班次：%1").arg(previousShift);
        shiftDisplayButton->setStyleSheet(
            "QPushButton {"
            "font-size: 12pt;"
            "font-weight: bold;"
            "padding: 8px 16px;"
            "min-width: 100px;"
            "border: 2px solid #FF9800;"
            "border-radius: 5px;"
            "background-color: #FFF3E0;"
            "}"
            "QPushButton:hover {"
            "background-color: #FFE0B2;"
            "}"
        );
    }
    
    shiftDisplayButton->setText(buttonText);
}

/**
 * @brief 班次显示按钮点击处理
 */
void tcpClient::onShiftDisplayButtonClicked()
{
    if (m_currentDisplayShift == "current") {
        // 切换到前一个班次
        m_currentDisplayShift = "previous";
        loadPreviousShiftStatistics();
        updateShiftDisplayButton();
        
        // 启动自动恢复定时器
        m_shiftDisplayAutoResetTimer->start();
        
        appendToLog("已切换到前一个班次数据", false);
    } else {
        // 切换回当前班次
        restoreCurrentShiftDisplay();
    }
}

/**
 * @brief 加载前一个班次的统计信息
 */
void tcpClient::loadPreviousShiftStatistics()
{
    // 检查数据库是否已打开
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "数据库未打开，跳过加载前一个班次统计信息";
        return;
    }
    
    QString currentShift = getCurrentShift();
    QString previousShift = (currentShift == "白班") ? "夜班" : "白班";
    
    QSqlQuery query;
    // 查询前一个班次的最新记录
    query.prepare("SELECT actual_count FROM shift_records WHERE shift_type = ? ORDER BY create_time DESC LIMIT 1");
    query.addBindValue(previousShift);
    
    if (!query.exec()) {
        appendToLog(QString("查询前一个班次（%1）记录失败: %2").arg(previousShift).arg(query.lastError().text()), true);
        qWarning() << "查询前一个班次记录失败:" << query.lastError().text();
        return;
    }
    
    // 获取前一个班次的实际便次
    int previousActualCount = 0;
    if (query.next()) {
        previousActualCount = query.value(0).toInt();
    }
    
    // 更新显示的统计信息（前一个班次只有实际便次，计划便次和延迟便次保持当前值）
    m_displayedActualCount = previousActualCount;
    m_displayedPlannedCount = m_plannedCount; // 计划便次保持当前值
    m_displayedDelayedCount = m_delayedCount; // 延迟便次保持当前值
    
    // 更新标签显示
    if (plannedCountLabel) {
        plannedCountLabel->setText(QString("计划便次：%1便").arg(m_displayedPlannedCount));
    }
    if (actualCountLabel) {
        actualCountLabel->setText(QString("实际便次：%1便").arg(m_displayedActualCount));
    }
    if (delayedCountLabel) {
        delayedCountLabel->setText(QString("延迟便次：%1便").arg(m_displayedDelayedCount));
    }
    
    qDebug() << QString("已加载前一个班次（%1）数据：实际便次=%2").arg(previousShift).arg(previousActualCount);
}

/**
 * @brief 恢复显示当前班次数据
 */
void tcpClient::restoreCurrentShiftDisplay()
{
    // 检查是否与当前班次不一致
    QString currentShift = getCurrentShift();
    
    if (m_currentDisplayShift == "previous") {
        // 恢复显示当前班次数据
        m_currentDisplayShift = "current";
        m_displayedPlannedCount = m_plannedCount;
        m_displayedActualCount = m_actualCount;
        m_displayedDelayedCount = m_delayedCount;
        
        // 更新标签显示
        if (plannedCountLabel) {
            plannedCountLabel->setText(QString("计划便次：%1便").arg(m_displayedPlannedCount));
        }
        if (actualCountLabel) {
            actualCountLabel->setText(QString("实际便次：%1便").arg(m_displayedActualCount));
        }
        if (delayedCountLabel) {
            delayedCountLabel->setText(QString("延迟便次：%1便").arg(m_displayedDelayedCount));
        }
        
        updateShiftDisplayButton();
        
        // 停止自动恢复定时器
        m_shiftDisplayAutoResetTimer->stop();
        
        appendToLog(QString("已恢复显示当前班次（%1）数据").arg(currentShift), false);
    }
}

/**
 * @brief 更新工程组统计表格
 */
void tcpClient::updateProjectGroupStatistics()
{
    if (!projectGroupTable) {
        return;
    }
    
    // 检查数据库是否已打开
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        qDebug() << "数据库未打开，跳过更新工程组统计";
        return;
    }
    
    // 获取所有车型名称（从车型绑定表）
    QSqlQuery query;
    query.prepare("SELECT DISTINCT model_name FROM model_bindings ORDER BY model_name");
    
    if (!query.exec()) {
        appendToLog(QString("查询车型列表失败: %1").arg(query.lastError().text()), true);
        qWarning() << "查询车型列表失败:" << query.lastError().text();
        return;
    }
    
    QMap<QString, QMap<QString, int>> statistics; // 车型名称 -> {操作类型 -> 数量}
    
    // 初始化所有车型的统计数据
    while (query.next()) {
        QString modelName = query.value(0).toString();
        statistics[modelName]["实托盘搬入"] = 0;
        statistics[modelName]["实托盘搬出"] = 0;
        statistics[modelName]["空托盘搬入"] = 0;
        statistics[modelName]["空托盘搬出"] = 0;
    }
    
    // 根据显示的班次决定统计哪个班次的数据
    QString currentShift = getCurrentShift();
    QString displayShift = m_projectGroupDisplayShift == "previous" ? 
        ((currentShift == "白班") ? "夜班" : "白班") : currentShift;
    
    QDateTime now = QDateTime::currentDateTime();
    
    // 计算要显示的班次的时间范围
    QDateTime shiftStartTime, shiftEndTime;
    if (displayShift == "白班") {
        // 白班：今天7:15 - 今天17:30
        shiftStartTime = QDateTime(now.date(), QTime(7, 15, 0));
        shiftEndTime = QDateTime(now.date(), QTime(17, 30, 0));
    } else {
        // 夜班：昨天17:30 - 今天7:15
        shiftStartTime = QDateTime(now.date().addDays(-1), QTime(17, 30, 0));
        shiftEndTime = QDateTime(now.date(), QTime(7, 15, 0));
    }
    
    // 如果是显示前一个班次，需要调整时间范围到前一个班次
    if (m_projectGroupDisplayShift == "previous") {
        if (displayShift == "白班") {
            // 前一个白班：昨天7:15 - 昨天17:30
            shiftStartTime = QDateTime(now.date().addDays(-1), QTime(7, 15, 0));
            shiftEndTime = QDateTime(now.date().addDays(-1), QTime(17, 30, 0));
        } else {
            // 前一个夜班：前天17:30 - 昨天7:15
            shiftStartTime = QDateTime(now.date().addDays(-2), QTime(17, 30, 0));
            shiftEndTime = QDateTime(now.date().addDays(-1), QTime(7, 15, 0));
        }
    }
    
    // 从数据记录表统计各车型的操作次数
    query.prepare("SELECT model_name, status, time FROM data_records WHERE status IN ('实托盘搬入', '实托盘搬出', '空托盘搬入', '空托盘搬出')");
    
    if (!query.exec()) {
        appendToLog(QString("查询统计数据失败: %1").arg(query.lastError().text()), true);
        qWarning() << "查询统计数据失败:" << query.lastError().text();
        return;
    }
    
    while (query.next()) {
        QString modelName = query.value(0).toString();
        QString status = query.value(1).toString();
        QString timeStr = query.value(2).toString();
        
        // 解析时间字符串（格式：yyyy-MM-dd hh:mm:ss 或 yyyy-MM-dd HH:mm:ss）
        QDateTime recordTime = QDateTime::fromString(timeStr, "yyyy-MM-dd HH:mm:ss");
        if (!recordTime.isValid()) {
            // 尝试12小时制格式
            recordTime = QDateTime::fromString(timeStr, "yyyy-MM-dd hh:mm:ss");
        }
        
        if (!recordTime.isValid()) {
            qWarning() << "无法解析时间字符串:" << timeStr;
            continue;
        }
        
        // 判断记录是否属于要显示的班次
        bool isInShift = false;
        if (displayShift == "白班") {
            // 白班：7:15 - 17:30（同一天）
            isInShift = (recordTime >= shiftStartTime && recordTime < shiftEndTime);
        } else {
            // 夜班：17:30 - 次日7:15（跨天）
            isInShift = (recordTime >= shiftStartTime && recordTime < shiftEndTime);
        }
        
        if (isInShift && statistics.contains(modelName)) {
            statistics[modelName][status]++;
        }
    }
    
    // 更新表格
    projectGroupTable->setRowCount(statistics.size());
    int row = 0;
    
    // 按车型名称排序
    QList<QString> modelNames = statistics.keys();
    std::sort(modelNames.begin(), modelNames.end());
    
    for (const QString& modelName : modelNames) {
        // 车型名称
        QTableWidgetItem* nameItem = new QTableWidgetItem(modelName);
        nameItem->setTextAlignment(Qt::AlignCenter);
        projectGroupTable->setItem(row, 0, nameItem);
        
        // 实托盘搬入
        QTableWidgetItem* realInItem = new QTableWidgetItem(QString::number(statistics[modelName]["实托盘搬入"]));
        realInItem->setTextAlignment(Qt::AlignCenter);
        projectGroupTable->setItem(row, 1, realInItem);
        
        // 实托盘搬出
        QTableWidgetItem* realOutItem = new QTableWidgetItem(QString::number(statistics[modelName]["实托盘搬出"]));
        realOutItem->setTextAlignment(Qt::AlignCenter);
        projectGroupTable->setItem(row, 2, realOutItem);
        
        // 空托盘搬入
        QTableWidgetItem* emptyInItem = new QTableWidgetItem(QString::number(statistics[modelName]["空托盘搬入"]));
        emptyInItem->setTextAlignment(Qt::AlignCenter);
        projectGroupTable->setItem(row, 3, emptyInItem);
        
        // 空托盘搬出
        QTableWidgetItem* emptyOutItem = new QTableWidgetItem(QString::number(statistics[modelName]["空托盘搬出"]));
        emptyOutItem->setTextAlignment(Qt::AlignCenter);
        projectGroupTable->setItem(row, 4, emptyOutItem);
        
        row++;
    }
    
    qDebug() << QString("工程组统计表格已更新，共%1个车型").arg(statistics.size());
}

/**
 * @brief 工程组记录界面班次按钮点击处理
 */
void tcpClient::onProjectGroupShiftButtonClicked()
{
    if (m_projectGroupDisplayShift == "current") {
        // 切换到前一个班次
        m_projectGroupDisplayShift = "previous";
        QString currentShift = getCurrentShift();
        QString previousShift = (currentShift == "白班") ? "夜班" : "白班";
        
        // 更新按钮文本
        if (projectGroupShiftButton) {
            projectGroupShiftButton->setText(previousShift);
        }
        
        // 更新统计表格
        updateProjectGroupStatistics();
        
        // 启动自动恢复定时器
        m_projectGroupShiftAutoResetTimer->start();
        
        appendToLog(QString("工程组记录已切换到前一个班次（%1）数据").arg(previousShift), false);
    } else {
        // 切换回当前班次
        m_projectGroupDisplayShift = "current";
        QString currentShift = getCurrentShift();
        
        // 更新按钮文本
        if (projectGroupShiftButton) {
            projectGroupShiftButton->setText(currentShift);
        }
        
        // 停止自动恢复定时器
        m_projectGroupShiftAutoResetTimer->stop();
        
        // 更新统计表格
        updateProjectGroupStatistics();
        
        appendToLog(QString("工程组记录已切换回当前班次（%1）数据").arg(currentShift), false);
    }
}

/**
 * @brief 工程组记录界面班次自动恢复处理
 */
void tcpClient::onProjectGroupShiftAutoReset()
{
    if (m_projectGroupDisplayShift == "previous") {
        // 自动恢复为当前班次
        m_projectGroupDisplayShift = "current";
        QString currentShift = getCurrentShift();
        
        // 更新按钮文本
        if (projectGroupShiftButton) {
            projectGroupShiftButton->setText(currentShift);
        }
        
        // 更新统计表格
        updateProjectGroupStatistics();
        
        appendToLog(QString("工程组记录已自动恢复为当前班次（%1）数据").arg(currentShift), false);
    }
}




