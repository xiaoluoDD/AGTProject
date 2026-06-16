#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QWidget>
#include <QTcpSocket>
#include <QTimer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDateTime>
#include <QLabel>
#include <QLoggingCategory>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QStandardPaths>
#include <QMap>
#include <QVector>
#include <QGroupBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QTableWidget>
#include <QInputDialog>
#include <QDate>
#include <QTime>
#include <QHeaderView>
#include <QPainter>

// 前向声明
class QTableWidgetItem;
class QPushButton;
class QTimeEdit;

/**
 * @brief 自定义QHeaderView类，支持二级表头
 */
class TwoLevelHeaderView : public QHeaderView
{
public:
    explicit TwoLevelHeaderView(Qt::Orientation orientation, QWidget *parent = nullptr);

    // 设置一级表头文本
    void setFirstLevelHeader(int firstLevelIndex, const QString &text);
    // 设置二级表头文本
    void setSecondLevelHeader(int section, const QString &text);
    // 设置列索引到一级表头索引的映射
    void setSectionToFirstLevel(int section, int firstLevelIndex);
    // 获取二级表头文本
    QString getSecondLevelHeader(int section) const;
    // 获取一级表头文本
    QString getFirstLevelHeader(int section) const;
    // 获取列索引对应的一级表头索引
    int getFirstLevelIndex(int section) const;
    /// 在 logicalIndex 处插入列之前，将 >= insertPos 的 section 映射整体右移 1
    void shiftSectionMapsBeforeInsert(int insertPos);
    /// 删除 logicalIndex 列之后，移除该 section 的映射并将更大索引左移 1
    void shiftSectionMapsAfterRemove(int removedSection);

protected:
    void paintEvent(QPaintEvent *e) override;
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;
    QSize sectionSizeFromContents(int logicalIndex) const override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    QMap<int, QString> m_firstLevelHeaders;  // 一级表头文本（firstLevelIndex -> text）
    QMap<int, QString> m_secondLevelHeaders; // 二级表头文本（section -> text）
    QMap<int, int> m_sectionToFirstLevel;     // 列索引到一级表头索引的映射（section -> firstLevelIndex）
};

QT_BEGIN_NAMESPACE
namespace Ui {
class tcpClient;
}
QT_END_NAMESPACE

/**
 * @brief TCP客户端主窗口类
 *
 * 提供TCP连接管理、数据发送接收、16进制数据处理等功能
 */
class tcpClient : public QWidget
{
    Q_OBJECT

public:
    tcpClient(QWidget *parent = nullptr);
    ~tcpClient();

signals:
    // 数据解析完成信号
    void dataParsed(int status1, unsigned int value1, unsigned int value2,
                    int status2, unsigned int value3, unsigned int value4,
                    const QString &currentTime);

private slots:
    // 界面切换槽函数
    void onConnectionPageClicked(); ///< 切换到连接界面
    void onCurrentShiftTablePageClicked(); ///< 切换到当前班次表格界面
    void onTablePageClicked();      ///< 切换到表格界面
    void onVisualizationPageClicked(); ///< 切换到可视化记录界面
    void onProjectGroupPageClicked(); ///< 切换到工程组记录界面
    void onAssemblyIndicatorPageClicked(); ///< 切换到总成指示表界面
    void onVehicleBindingPageClicked(); ///< 切换到车型绑定界面
    void updateProjectGroupStatistics(); ///< 更新工程组统计表格
    void loadVehicleModelsToAssemblyIndicator(); ///< 加载绑定车型到总成指示表
    void onOvertimeTimeButtonClicked(); ///< 加班时间选择按钮点击处理
    void addOvertimeColumns(double hours); ///< 添加加班时间列到总成指示表（插在合计列之前）
    void updatePlanRowsForOvertimeColumns(int startCol); ///< 更新所有计划行在加班列中的值
    int assemblyIndicatorTotalColumnIndex() const; ///< 最后一列「总装引取合计」的列索引（columnCount()-1）
    void updateAssemblyPickupTotalForRow(int assemblyRow); ///< 汇总时间列更新总装引取合计
    void removeAssemblyIndicatorOvertimeColumns(int columns); ///< 从列39起删除若干加班列（保留末尾合计）
    void onCurrentTableButtonClicked(); ///< 当前表格按钮点击处理
    void onHistoryTableButtonClicked(); ///< 历史表格按钮点击处理
    void updateTableModeButtons(bool isHistoryMode); ///< 更新按钮选中状态和显示/隐藏班次时间按钮
    void onSelectHistoryDateShiftClicked(); ///< 选择历史数据时间和班次按钮点击处理
    void onExportHistoryTableClicked(); ///< 导出历史表格按钮点击处理
    void exportAssemblyIndicatorTableToCSV(); ///< 导出总装指示表数据为CSV文件
    void exportAssemblyIndicatorTableToHTML(); ///< 导出总装指示表数据为Excel文件（.xls格式，支持单元格合并）
    void loadAssemblyIndicatorHistoryFromDb(const QDate& date, const QString& shiftType); ///< 从历史表加载总装指示表数据
    void updateAssemblyIndicatorActualRow(const QString& vehicleName); ///< 更新总成指示表的实际行（实托盘搬出时调用）

    // 连接管理槽函数
    void onConnectClicked();      ///< 连接按钮点击处理
    void onDisconnectClicked();   ///< 断开按钮点击处理
    void onSendClicked();         ///< 发送按钮点击处理
    void onClearClicked();        ///< 清空日志按钮点击处理

    // 表格操作槽函数
    void onClearTableClicked();   ///< 清空表格按钮点击处理
    void onDeleteTableClicked();  ///< 删除选中表格按钮点击处理
    void onExportTableClicked();  ///< 导出表格按钮点击处理
    void onClearCurrentShiftTableClicked();   ///< 清空当前班次表格按钮点击处理
    void onDeleteCurrentShiftTableClicked();  ///< 删除选中当前班次表格按钮点击处理
    void onTableHeaderClicked(int logicalIndex); ///< 表格表头点击处理（筛选功能）
    void applyTableFilter(); ///< 应用表格筛选
    void clearTableFilter(); ///< 清除表格筛选
    void updateTableHeaderFilterIndicator(); ///< 更新表头筛选指示器

    // 车型绑定表格操作槽函数
    void onAddVehicleClicked();   ///< 添加车型按钮点击处理
    void onDeleteVehicleClicked(); ///< 删除选中车型按钮点击处理
    void onClearVehicleTableClicked(); ///< 清空车型表格按钮点击处理
    void onExportVehicleTableClicked(); ///< 导出车型表格按钮点击处理
    void onVehicleBindingItemChanged(QTableWidgetItem *item); ///< 车型绑定表格数据变化处理
    void onAssemblyIndicatorItemChanged(QTableWidgetItem *item); ///< 总装指示表数据变化处理

    // 数据解析槽函数
    void onDataParsed(int status1, unsigned int value1, unsigned int value2,
                      int status2, unsigned int value3, unsigned int value4,
                      const QString &currentTime); ///< 处理解析完成的数据

    // 网络事件槽函数
    void onSocketConnected();     ///< Socket连接成功处理
    void onSocketDisconnected();  ///< Socket断开连接处理
    void onSocketError(QAbstractSocket::SocketError error); ///< Socket错误处理
    void onSocketReadyRead();     ///< Socket数据可读处理
    void onConnectionTimeout();   ///< 连接超时处理
    void onPlcAutoReconnect();    ///< PLC自动重连处理
    void onCurrentShiftTableDailyClear(); ///< 当前班次表格每日清空处理（每天凌晨6点）
    void onServerSocketConnected();     ///< 服务端Socket连接成功处理
    void onServerSocketDisconnected();  ///< 服务端Socket断开连接处理
    void onServerSocketError(QAbstractSocket::SocketError error); ///< 服务端Socket错误处理
    void onServerSocketReadyRead();     ///< 服务端Socket数据可读处理
    void onServerConnectionTimeout();   ///< 服务端连接超时处理
    void onServerAutoReconnect();       ///< 服务端自动重连处理
    void onServerHeartbeat();           ///< 服务端心跳发送处理（发送ping）
    void onServerHeartbeatPongTimeout(); ///< 服务端心跳应答超时处理（重发3次无应答则弹窗提示）
    void onEdSoftwareAutoReconnect();   ///< ED软件自动重连处理
    void startAutoConnect();            ///< 启动时自动连接（加载数据库/连接配置后调用）

    // 密码管理槽函数
    void onSetPasswordClicked();  ///< 设置密码按钮点击处理

    // 数据库配置槽函数
    void onSaveDatabaseConfigClicked(); ///< 保存数据库配置按钮点击处理
    void onTestDatabaseConnectionClicked(); ///< 测试数据库连接按钮点击处理

    // 班次设置槽函数
    void onSaveShiftConfigClicked(); ///< 保存班次设置按钮点击处理

    // 连接配置槽函数
    void onSavePlcConnectionConfigClicked(); ///< 保存PLC连接配置按钮点击处理
    void onSaveServerConnectionConfigClicked(); ///< 保存服务端连接配置按钮点击处理
    void onServerConnectClicked(); ///< 服务端连接按钮点击处理
    void onServerDisconnectClicked(); ///< 服务端断开按钮点击处理
    void onEdSoftwareConnectClicked(); ///< ED软件连接按钮点击处理
    void onEdSoftwareDisconnectClicked(); ///< ED软件断开按钮点击处理
    void onSaveEdSoftwareConnectionConfigClicked(); ///< 保存ED软件连接配置按钮点击处理
    void onExceptionPopupSwitchToggled(bool checked); ///< 异常弹窗提示开关切换处理
    void onEdSoftwareSocketConnected(); ///< ED软件Socket连接成功处理
    void onEdSoftwareSocketDisconnected(); ///< ED软件Socket断开连接处理
    void onEdSoftwareSocketError(QAbstractSocket::SocketError error); ///< ED软件Socket错误处理
    void onEdSoftwareSocketReadyRead(); ///< ED软件Socket数据可读处理
    void onEdSoftwareConnectionTimeout(); ///< ED软件连接超时处理
    void updateEdSoftwareConnectionStatus(bool connected); ///< 更新ED软件连接状态显示

    // 统计信息双击编辑槽函数
    void onPlannedCountLabelDoubleClicked();   ///< 第一节便次标签双击处理
    void onActualCountLabelDoubleClicked();    ///< 第二节便次标签双击处理
    void onDelayedCountLabelDoubleClicked();   ///< 第三节便次标签双击处理
    void onSection4CountLabelDoubleClicked();  ///< 第四节便次标签双击处理
    void onTotalCountLabelDoubleClicked();     ///< 总数便次标签双击处理
    void updateTotalCountDisplay();            ///< 更新总数便次显示（总数=第一节+第二节+第三节+第四节）

    // 滑槽标签双击编辑函数
    void onTraySlotLabelDoubleClicked(QLabel* label, bool isRealTray, int slotIndex); ///< 滑槽标签双击处理
    void onRealEntranceLongPressTimeout();  ///< 实滑槽「三车间滑槽出口」长按3秒超时：弹出清空确认
    void onEmptyEntranceLongPressTimeout(); ///< 空滑槽「三车间滑槽入口」长按3秒超时：弹出清空确认
    void onProjectGroupShiftButtonClicked(); ///< 工程组记录界面班次按钮点击处理
    void onProjectGroupShiftAutoReset(); ///< 工程组记录界面班次自动恢复处理

private:
    // UI和状态管理
    void setupUI();               ///< 初始化用户界面
    void setupTable();            ///< 初始化表格
    void setupVehicleBindingTable(); ///< 初始化车型绑定表格
    void updateConnectionStatus(bool connected); ///< 更新连接状态显示
    void appendToLog(const QString &message, bool isError = false); ///< 添加日志信息
    void applyModernStyle();      ///< 应用现代化样式
    void applyBuiltinStyle();     ///< 应用内置样式

    // 数据处理
    void sendData(const QByteArray &data); ///< 发送数据到服务器
    void processHexData(const QByteArray &data); ///< 处理16进制数据
    void updateVisualization(const QString &vehicleName, bool isRealTray, int slotIndex = -1); ///< 更新可视化界面，slotIndex为-1时使用批次计数，0-2时直接指定位置
    void advanceVisualization(); ///< 推进可视化显示
    void advanceVisualizationBy3(); ///< 推进可视化显示3个位置
    void addDataToTable(int status1, unsigned int value1, unsigned int value2,
                        int status2, unsigned int value3, unsigned int value4,
                        const QString &currentTime); ///< 添加数据到表格
    void addDataToCurrentShiftTable(int slotNo, const QString &status, const QString &modelCode,
                                    const QString &modelName, int count, const QString &currentTime,
                                    int plcStripeBatchId = -1); ///< plcStripeBatchId>=0 时同值表示同一条 PLC 报文，用于行底色分组；-1 表示不设置（如从库加载时用行号回退）

    void initDatabase();
    void loadDatabaseConfig(); ///< 从配置文件加载数据库配置
    void saveDatabaseConfig(); ///< 保存数据库配置到配置文件
    bool testDatabaseConnection(const QString &host, int port, const QString &database,
                                const QString &username, const QString &password); ///< 测试数据库连接
    void loadModelBindingsFromDb();
    void loadDataRecordsFromDb();
    void insertDataRecord(int slotNo, const QString &status, const QString &modelName, const QString &modelCode, int count, const QString &currentTime,
                          const QString &recordSource = QString(), int plcStripeBatch = -1); ///< recordSource 空=plc；ed=ED；plcStripeBatch>=0 写入库供重启后当前班次条纹着色
    void deleteDataRecord(int slotNo, const QString &status, const QString &modelName, const QString &modelCode, int count, const QString &currentTime);
    void insertModelBinding(const QString &modelCode, const QString &modelName, int count,
                            const QString &assemblyNumber = QString());
    void updateModelBinding(const QString &oldModelCode, const QString &oldModelName, int oldCount, int row);
    void deleteModelBinding(const QString &modelName);
    void clearModelBindings();
    void clearDataRecords();
    void insertCurrentShiftRecord(int slotNo, const QString &status, const QString &modelCode, const QString &modelName, int count, const QString &currentTime); ///< 插入当前班次表格记录到数据库
    void deleteCurrentShiftRecord(int slotNo, const QString &status, const QString &modelCode, const QString &modelName, int count, const QString &currentTime); ///< 从数据库删除当前班次表格记录
    void clearCurrentShiftRecords(); ///< 清空当前班次表格数据库记录
    void loadCurrentShiftRecordsFromDb(); ///< 从数据库加载当前班次表格记录
    void saveVisualizationRecords(); ///< 保存可视化记录到数据库
    void loadVisualizationRecords(); ///< 从数据库加载可视化记录
    void saveStatisticsInfo(); ///< 保存统计信息到数据库
    void loadStatisticsInfo(); ///< 从数据库加载统计信息
    void saveAssemblyIndicatorToDb(bool showMessageBox = true); ///< 保存总装指示表参量设置到数据库（只保存产量和节拍）
    void loadAssemblyIndicatorFromDb(const QDate& date, const QString& shiftType); ///< 从数据库加载总装指示表
    void saveAssemblyIndicatorCurrentShift(); ///< 保存当前班次数据到当前班次表
    void clearAssemblyIndicatorCurrentShift(const QDate& date, const QString& shiftType); ///< 清空总成指示表当前班次的数据（数据库）
    void saveAssemblyIndicatorHistory(); ///< 保存历史数据到历史表（每次PLC数据更新时调用）
    QString getCurrentShift(); ///< 获取当前班次（"白班"或"夜班"）
    void onShiftDisplayButtonClicked(); ///< 班次显示按钮点击处理
    void loadPreviousShiftStatistics(); ///< 加载前一个班次的统计信息
    void restoreCurrentShiftDisplay(); ///< 恢复显示当前班次数据
    void updateShiftDisplayButton(); ///< 更新班次显示按钮文本
    void initShiftTable(); ///< 初始化班次记录表
    void checkShiftChange(); ///< 检查班次变化
    void saveShiftRecord(const QString &shiftType); ///< 保存班次记录到数据库
    QStringList getVehicleModelList(); ///< 获取所有绑定的车型名称列表
    void saveConnectionConfig(const QString &configType, const QString &ip, int port); ///< 保存连接配置到数据库
    void loadConnectionConfig(); ///< 从数据库加载连接配置
    void initShiftConfigTable(); ///< 初始化班次设置表
    void saveShiftConfig(); ///< 保存班次设置到数据库
    void loadShiftConfig(); ///< 从数据库加载班次设置
    void updateServerConnectionStatus(bool connected); ///< 更新服务端连接状态显示
    int findCompleteJsonObject(const QByteArray &buffer); ///< 查找缓冲区中第一个完整的JSON对象
    void processServerJsonData(const QByteArray &data, bool saveToTable = true); ///< 处理服务端JSON数据（支持多个连续的JSON对象）
    bool processSingleJsonObject(const QString &jsonString, bool saveToTable = true); ///< 处理单个JSON对象
    bool processProductionDataJson(const QJsonObject &obj, bool saveToTable = true); ///< 处理生产数据上报JSON
    bool processVehiclePartsJson(const QJsonObject &obj); ///< 服务端整包同步车型绑定（vehicle_parts）
    bool processAssemblyInstructionJson(const QJsonObject &obj, bool saveToTable = true); ///< 总装指令：依车型+时间戳定位时段列，每条消息+1（忽略次数字段）
    bool processAssemblyPickupFullSyncJson(const QJsonObject &obj, bool saveToTable = true); ///< 处理总装引取全量同步（ED）
    bool isDateTimeInCurrentShiftWindow(const QDateTime &recordDateTime); ///< 时间是否落在当前班次窗口内
    int findAssemblyIndicatorColumnForDateTime(const QDateTime &dateTime, const QString &shiftType); ///< 总成表时间映射列；未落入任一时段时归到「前一时段」末列
    void sendVisualizationDataToServer(); ///< 发送可视化数据到服务端
    QJsonObject buildVisualizationData(); ///< 构建可视化数据JSON对象
    void sendProjectGroupDataToServer(); ///< 发送工程组数据到服务端
    QJsonObject buildProjectGroupData(); ///< 构建工程组数据JSON对象
    void sendOvertimeInfoToServer(); ///< 发送加班时间信息到服务端
    void sendExceptionDataToServer(); ///< 发送异常记录数据到服务端
    QJsonObject buildExceptionData(); ///< 构建异常记录数据JSON对象（当前班次）
    void handleRealTrayIn(const QString &modelName, int slotNumber = -1); ///< 处理实托盘搬入，slotNumber为-1时使用原逻辑，否则根据slotNumber确定位置
    void handleEmptyTrayIn(const QString &modelName, int slotNo = -1); ///< 处理空托盘搬入，slotNo为-1时使用原逻辑，否则根据slotNo确定位置
    void handleEmptyTrayOut(const QString &modelName, int slotNumber = -1); ///< 处理空托盘搬出，slotNumber为-1时使用原逻辑，否则根据slotNumber确定位置
    void advanceEmptyTrayVisualization(); ///< 推进空托盘可视化显示
    void advanceEmptyTrayVisualizationBy3(const QString &modelName = ""); ///< 推进空托盘可视化显示3个位置，modelName为新进入的车型名称（用于记录异常）
    void saveEmptyTrayVisualizationRecords(); ///< 保存空托盘可视化记录到数据库
    void loadEmptyTrayVisualizationRecords(); ///< 从数据库加载空托盘可视化记录
    void clearAllRealTrayVisualizationSlots();  ///< 清空实滑槽全部车型及数据库 visualization_records
    void clearAllEmptyTrayVisualizationSlots(); ///< 清空空滑槽全部车型及数据库 empty_tray_visualization_records
    int mapSlotNumberToActualSlot(int slotNumber); ///< 将指令滑槽号映射为实际滑槽号（1->2001, 2->2002, 3->2003, 4->2103, 5->2102, 6->2101）
    int getSlotNoFromIndex(int slotIndex, bool isRealTray); ///< 根据槽位索引获取滑槽号（实托盘：索引18、19、20对应2101、2102、2103；空托盘：索引18、19、20对应2001、2002、2003）

    // 密码管理函数
    void initPasswordTable();     ///< 初始化密码表
    void savePassword(const QString &password); ///< 保存密码到数据库
    QString loadPassword();       ///< 从数据库加载密码
    bool verifyPassword(const QString &inputPassword); ///< 验证密码
    bool showPasswordDialog(const QString &title, const QString &message); ///< 显示密码输入对话框
    void updatePasswordDisplay(); ///< 更新密码显示

    // 日志系统函数
    void initLogSystem();         ///< 初始化日志系统
    void setupLogDirectory();     ///< 设置日志目录
    void setupLogFile();          ///< 设置日志文件
    void reopenLogFileForDate(const QDate& date); ///< 按指定日期打开/切换日志文件（不调用 qDebug，供消息处理器内使用）
    void ensureLogFileForCurrentDate(); ///< 若已跨天则切换到当日日志文件
    void cleanupLogFiles();       ///< 清理旧日志文件
    static void logMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg); ///< 日志消息处理器

    // 事件处理
    void closeEvent(QCloseEvent *event) override; ///< 重写关闭事件
    bool eventFilter(QObject *obj, QEvent *event) override; ///< 事件过滤器，用于处理标签双击

private:
    Ui::tcpClient *ui;            ///< UI界面指针
    QTcpSocket *m_socket;         ///< PLC TCP Socket对象
    QTcpSocket *m_serverSocket;  ///< 服务端 TCP Socket对象
    QByteArray m_serverDataBuffer; ///< 服务端数据缓冲区（用于处理TCP分包）
    QTcpSocket *m_edSoftwareSocket; ///< ED软件 TCP Socket对象
    QTimer *m_connectionTimer;    ///< PLC连接超时定时器
    QTimer *m_serverConnectionTimer; ///< 服务端连接超时定时器
    QTimer *m_edSoftwareConnectionTimer; ///< ED软件连接超时定时器
    QTimer *m_shiftCheckTimer;   ///< 班次检查定时器（每分钟检查一次）
    QTimer *m_visualizationDataTimer; ///< 可视化数据发送定时器（每3秒触发一次，立即发送AGT搬运数据，1秒后发送工程组数据，2秒后发送异常记录）
    QTimer *m_projectGroupDataTimer; ///< 工程组数据发送定时器（单次触发，3秒后发送工程组数据）
    QTimer *m_exceptionDataTimer; ///< 异常数据发送定时器（单次触发，发送工程组后3秒发送异常记录）
    QTimer *m_shiftDisplayAutoResetTimer; ///< 班次显示自动恢复定时器（1分钟后恢复）
    QTimer *m_plcAutoReconnectTimer; ///< PLC自动重连定时器（每3秒检测一次）
    QTimer *m_serverAutoReconnectTimer; ///< 服务端自动重连定时器（每3秒检测一次）
    QTimer *m_serverHeartbeatTimer;     ///< 服务端心跳定时器（每30秒发送ping）
    QTimer *m_serverHeartbeatPongTimer; ///< 服务端心跳应答超时定时器（重发3次无应答则弹窗）
    int m_serverHeartbeatRetryCount;    ///< 服务端心跳重发次数（收到pong时清零）
    QTimer *m_edSoftwareAutoReconnectTimer; ///< ED软件自动重连定时器（每3秒检测一次）
    QTimer *m_currentShiftTableDailyClearTimer; ///< 当前班次表格每日清空定时器（每分钟检查一次，凌晨6点清空）
    QTimer *m_projectGroupShiftAutoResetTimer; ///< 工程组记录界面班次自动恢复定时器（1分钟后恢复）
    bool m_isConnected;           ///< PLC连接状态标志
    bool m_isServerConnected;     ///< 服务端连接状态标志
    bool m_isEdSoftwareConnected; ///< ED软件连接状态标志
    bool m_isAutoReconnecting;   ///< 是否正在自动重连标志（PLC）
    bool m_plcManualDisconnect;  ///< PLC是否为用户手动断开（手动断开后不自动重连）
    bool m_serverManualDisconnect; ///< 服务端是否为用户手动断开（手动断开后不自动重连）
    bool m_edSoftwareManualDisconnect; ///< ED软件是否为用户手动断开（手动断开后不自动重连）
    int m_fullTrayCount;          ///< 满托盘时数量
    QDateTime m_lastStatus1Time;  ///< 旧版本兼容，已废弃
    QDateTime m_lastStatus2Time; ///< 旧版本兼容，已废弃
    QMap<QString, QDateTime> m_lastTrayTime; ///< 每个托盘的最后处理时间（key格式: "real_1" 或 "empty_1"）
    QLabel* labelConnectionStatus; ///< PLC连接状态标签（右下角）
    QLabel* m_labelServerStatus; ///< 服务端连接状态标签（右下角）
    QLabel* m_labelEdSoftwareStatus; ///< ED软件连接状态标签（右下角）
    QPushButton* pushButtonCurrentShiftTablePage; ///< 当前班次表格页面按钮
    QWidget* currentShiftTablePage; ///< 当前班次表格页面
    QTableWidget* currentShiftTableWidget; ///< 当前班次表格（已废弃，保留用于兼容）
    QTableWidget* realTrayInTableWidget; ///< 实托盘搬入表格
    QTableWidget* realTrayOutTableWidget; ///< 实托盘搬出表格
    QTableWidget* emptyTrayInTableWidget; ///< 空托盘搬入表格
    QTableWidget* emptyTrayOutTableWidget; ///< 空托盘搬出表格
    QPushButton* pushButtonVisualizationPage; ///< 可视化记录页面按钮
    QWidget* visualizationPage; ///< 可视化记录页面
    QPushButton* pushButtonProjectGroupPage; ///< 工程组记录页面按钮
    QWidget* projectGroupPage; ///< 工程组记录页面
    QTableWidget* projectGroupTable; ///< 工程组记录表格
    QPushButton* projectGroupShiftButton; ///< 工程组统计区域班次显示按钮
    QPushButton* pushButtonAssemblyIndicatorPage; ///< 总成指示表页面按钮
    QWidget* assemblyIndicatorPage; ///< 总成指示表页面
    QTableWidget* assemblyIndicatorTable; ///< 总成指示表表格
    QPushButton* pushButtonOvertimeTime; ///< 加班时间选择按钮
    QPushButton* pushButtonSaveAssemblyIndicator; ///< 保存总装指示表按钮
    QPushButton* pushButtonCurrentTable; ///< 当前表格按钮
    QPushButton* pushButtonHistoryTable; ///< 历史表格按钮
    QPushButton* pushButtonSelectHistoryDateShift; ///< 选择历史数据的时间和班次按钮
    QPushButton* pushButtonExportHistoryTable; ///< 导出历史表格按钮
    double m_overtimeHours; ///< 当前选择的加班时间（小时）
    bool m_isHistoryTableMode; ///< 是否显示历史表格模式（true=历史表格，false=当前表格）
    QDate m_selectedHistoryDate; ///< 当前选择的历史日期
    QString m_selectedHistoryShift; ///< 当前选择的历史班次
    QVector<QLabel*> m_realTrayLabels; ///< 实滑槽标签（23个：入口1个 + 21个槽位 + 出口1个）
    QVector<QLabel*> m_emptyTrayLabels; ///< 空滑槽标签（23个：入口1个 + 21个槽位 + 出口1个）
    QVector<QString> m_realTraySlots; ///< 实滑槽每个位置显示的车型名称（21个槽位，位置0-20）
    QVector<QString> m_emptyTraySlots; ///< 空滑槽每个位置显示的车型名称（21个槽位，位置0-20）
    QString m_realTrayExitLabelText; ///< 实滑槽出口标签的初始文本（用于重置时恢复）
    QString m_emptyTrayExitLabelText; ///< 空滑槽出口标签的初始文本（用于重置时恢复）
    QGroupBox* groupBoxServerConnection; ///< 服务端连接设置GroupBox
    QLineEdit* lineEditServerIP; ///< 服务端IP输入框
    QLineEdit* lineEditServerPort; ///< 服务端端口输入框
    QPushButton* pushButtonSavePlcConfig; ///< 保存PLC连接配置按钮
    QPushButton* pushButtonSaveServerConfig; ///< 保存服务端连接配置按钮
    QPushButton* pushButtonServerConnect; ///< 服务端连接按钮
    QPushButton* pushButtonServerDisconnect; ///< 服务端断开按钮
    QLabel* labelServerConnectionStatus; ///< 服务端连接状态标签
    QGroupBox* groupBoxEdSoftwareConnection; ///< ED软件连接设置GroupBox
    QLineEdit* lineEditEdSoftwareIP; ///< ED软件IP输入框
    QLineEdit* lineEditEdSoftwarePort; ///< ED软件端口输入框
    QPushButton* pushButtonSaveEdSoftwareConfig; ///< 保存ED软件连接配置按钮
    QPushButton* pushButtonEdSoftwareConnect; ///< ED软件连接按钮
    QPushButton* pushButtonEdSoftwareDisconnect; ///< ED软件断开按钮
    QLabel* labelEdSoftwareConnectionStatus; ///< ED软件连接状态标签
    QGroupBox* groupBoxShiftConfig; ///< 班次设置GroupBox
    QTimeEdit* timeEditDayShiftStart; ///< 白班开始时间输入框
    QTimeEdit* timeEditDayShiftEnd; ///< 白班结束时间输入框
    QTimeEdit* timeEditNightShiftStart; ///< 夜班开始时间输入框
    QTimeEdit* timeEditNightShiftEnd; ///< 夜班结束时间输入框
    QPushButton* pushButtonSaveShiftConfig; ///< 保存班次设置按钮
    QCheckBox* checkBoxExceptionPopup; ///< 异常弹窗提示开关
    QString m_password;           ///< 存储的密码
    bool m_isPasswordSet;         ///< 密码是否已设置
    QDate m_lastCurrentShiftTableClearDate; ///< 上次清空当前班次表格的日期

    // 班次设置（从数据库读取）
    QTime m_dayShiftStart;        ///< 白班开始时间
    QTime m_dayShiftEnd;          ///< 白班结束时间
    QTime m_nightShiftStart;      ///< 夜班开始时间
    QTime m_nightShiftEnd;        ///< 夜班结束时间
    bool m_shiftConfigLoaded;     ///< 班次设置是否已加载

    // 统计信息：单一表格（项目 | 计划便次 | 实际便次 | 差异），行：第一节便次、第二节便次、吃饭时间、第三节便次、第四节便次、加班时间、总数便次
    QTableWidget* statisticsTableWidget; ///< 统计信息表格
    QLabel* plannedCountLabel;    ///< 保留用于兼容（可为null）
    QLabel* actualCountLabel;
    QLabel* delayedCountLabel;
    QLabel* section4CountLabel;
    QLabel* totalCountLabel;
    QPushButton* shiftDisplayButton; ///< 班次显示按钮
    void updateStatisticsTableDisplay(); ///< 刷新统计表格所有单元格
    QString getStatisticsRowTimeRange(int rowIndex) const; ///< 根据总成指示表时间列返回统计表某行对应的时间范围（第一节/第二节/吃饭/第三节/第四节/加班）
    int getPlanSumForSecondLevelValue(int secondLevelValue) const; ///< 总成指示表时间列（二级表头值120/230/350/460）所有计划行的和
    int getActualSumForSecondLevelValue(int secondLevelValue) const; ///< 总成指示表时间列所有实际行的和（用于启动时从表格恢复统计）
    int getActualSumAtOrBeforeColumn(int col) const; ///< 实际行在指定列或往前第一个有数据列的和（列无数据时往前找）
    int getColumnForCurrentTime(); ///< 根据当前时间返回对应时间列索引（含fallback到前一时段最后一列）
    int getPlanSumForOvertimeLastColumn() const; ///< 加班时间列最后一列所有计划行的和（无加班返回0）
    void syncStatisticsFromAssemblyIndicator(); ///< 从总成指示表实际行同步四节实际便次到统计表（启动时调用，加载到当前时间）
    int getSectionFromTimeColumn(int colIndex) const; ///< 根据总成指示表时间列索引返回便次节（1=第一节,2=第二节,3=第三节,4=第四节,0=吃饭/休息,5=加班）
    bool hasReachedStatisticsRow(int rowIndex); ///< 当前时间是否已到达统计表某行对应时段（未到则显示空）
    int m_planTotal;              ///< 计划便次（用于右侧表格及差异计算）
    int m_plannedCount;           ///< 第一节便次计划值
    int m_actualCount;             ///< 第二节便次（保留兼容，总数=四节实际便次之和）
    int m_delayedCount;            ///< 第三节便次
    int m_section4Count;           ///< 第四节便次
    int m_section1ActualCount;     ///< 第一节便次实际次数（仅空托盘搬出时按时间累加）
    int m_section2ActualCount;     ///< 第二节便次实际次数
    int m_section3ActualCount;     ///< 第三节便次实际次数
    int m_section4ActualCount;     ///< 第四节便次实际次数
    int m_mealActualCount;         ///< 吃饭时间实际次数（11:30-12:15/21:15-22:00 收到数据时累加）
    int m_totalCount;              ///< 总数便次数值
    QString m_currentDisplayShift; ///< 当前显示的班次（"current"表示当前班次，"previous"表示前一个班次）
    QString m_projectGroupDisplayShift; ///< 工程组记录界面显示的班次（"current"表示当前班次，"previous"表示前一个班次）
    int m_displayedPlannedCount;   ///< 显示的第一节便次（可能是前一个班次的）
    int m_displayedActualCount;    ///< 显示的第二节便次（可能是前一个班次的）
    int m_displayedDelayedCount;   ///< 显示的第三节便次（可能是前一个班次的）
    int m_displayedSection4Count;   ///< 显示的第四节便次
    int m_displayedTotalCount;     ///< 显示的总数便次
    int m_displayedSection1ActualCount;  ///< 显示的第一节实际便次（前班次用）
    int m_displayedSection2ActualCount;
    int m_displayedSection3ActualCount;
    int m_displayedSection4ActualCount;
    int m_displayedMealActualCount;     ///< 显示的吃饭时间实际便次（前班次用）

    // 实托盘批次处理相关
    int m_plcShiftTableStripeEpoch; ///< PLC 每条报文（processHexData / addDataToTable 一次调用）递增，当前班次表同报文多行同色
    int m_realTrayBatchCount;     ///< 当前批次已搬入的车型数量（0-3），0表示新批次开始
    QDateTime m_lastRealTrayOutTime; ///< 最后一次实托盘搬出的时间

    // 空托盘批次处理相关
    int m_emptyTrayBatchCount;    ///< 当前批次已搬出的车型数量（0-3），0表示新批次开始
    QTimer *m_realEntranceLongPressTimer;  ///< 实滑槽入口标签长按3秒定时器
    QTimer *m_emptyEntranceLongPressTimer; ///< 空滑槽入口标签长按3秒定时器

    QStringList m_pendingExceptionMessages; ///< 当前指令待弹窗的异常信息列表（一条指令只弹一次窗）
    bool m_enableExceptionPopup; ///< 是否启用异常弹窗提示

    // 数据库配置
    QString m_dbHost;             ///< 数据库主机地址
    int m_dbPort;                 ///< 数据库端口
    QString m_dbName;             ///< 数据库名称
    QString m_dbUsername;         ///< 数据库用户名
    QString m_dbPassword;         ///< 数据库密码

    // 日志系统成员变量
    QString m_logDirectory;       ///< 日志目录路径
    QString m_logFileName;        ///< 当前日志文件名
    QDate m_logFileDate;          ///< 当前日志文件对应的日历日期（用于跨天切换）
    QFile* m_logFile;             ///< 日志文件对象
    QTextStream* m_logStream;     ///< 日志流对象
    QMutex m_logMutex;            ///< 日志互斥锁
    static tcpClient* s_instance; ///< 静态实例指针（用于静态日志处理器）

    // 表格筛选相关
    QMap<int, QString> m_tableFilters; ///< 表格筛选条件（列索引 -> 筛选值）
    QList<QTableWidgetItem*> m_allTableItems; ///< 存储所有表格数据（用于筛选）

    // 异常表格相关
    QTableWidget* exceptionTableWidget; ///< 异常表格
    void insertExceptionRecord(int slotNo, const QString &modelName, const QString &status, const QString &exceptionInfo, const QString &date); ///< 插入异常记录到数据库
    void loadExceptionRecords(); ///< 从数据库加载异常记录（仅加载当前班次的记录）
    QString getShiftFromDateTime(const QDateTime &dateTime); ///< 根据日期时间获取班次（"白班"或"夜班"）
};

#endif // TCPCLIENT_H
