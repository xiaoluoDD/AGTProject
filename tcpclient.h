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
    void onTablePageClicked();      ///< 切换到表格界面
    void onVehicleBindingPageClicked(); ///< 切换到车型绑定界面
    
    // 连接管理槽函数
    void onConnectClicked();      ///< 连接按钮点击处理
    void onDisconnectClicked();   ///< 断开按钮点击处理
    void onSendClicked();         ///< 发送按钮点击处理
    void onClearClicked();        ///< 清空日志按钮点击处理
    
    // 表格操作槽函数
    void onClearTableClicked();   ///< 清空表格按钮点击处理
    void onExportTableClicked();  ///< 导出表格按钮点击处理
    
    // 车型绑定表格操作槽函数
    void onAddVehicleClicked();   ///< 添加车型按钮点击处理
    void onDeleteVehicleClicked(); ///< 删除选中车型按钮点击处理
    void onClearVehicleTableClicked(); ///< 清空车型表格按钮点击处理
    void onExportVehicleTableClicked(); ///< 导出车型表格按钮点击处理
    
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

private:
    // UI和状态管理
    void setupUI();               ///< 初始化用户界面
    void setupTable();            ///< 初始化表格
    void setupVehicleBindingTable(); ///< 初始化车型绑定表格
    void updateConnectionStatus(bool connected); ///< 更新连接状态显示
    void appendToLog(const QString &message, bool isError = false); ///< 添加日志信息
    
    // 数据处理
    void sendData(const QByteArray &data); ///< 发送数据到服务器
    void processHexData(const QByteArray &data); ///< 处理16进制数据
    void addDataToTable(int status1, unsigned int value1, unsigned int value2, 
                       int status2, unsigned int value3, unsigned int value4,
                       const QString &currentTime); ///< 添加数据到表格

    void initDatabase();
    void loadModelBindingsFromDb();
    void loadDataRecordsFromDb();
    void insertDataRecord(int slotNo, const QString &status, const QString &modelName, const QString &modelCode, int count, const QString &currentTime);
    void insertModelBinding(const QString &modelCode, const QString &modelName, int count);
    void deleteModelBinding(const QString &modelName);
    void clearModelBindings();
    void clearDataRecords();

private:
    Ui::tcpClient *ui;            ///< UI界面指针
    QTcpSocket *m_socket;         ///< TCP Socket对象
    QTimer *m_connectionTimer;    ///< 连接超时定时器
    bool m_isConnected;           ///< 连接状态标志
    int m_fullTrayCount;          ///< 满托盘时数量
    QDateTime m_lastStatus1Time;
    QDateTime m_lastStatus2Time;
};

#endif // TCPCLIENT_H
