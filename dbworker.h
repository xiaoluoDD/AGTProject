#ifndef DBWORKER_H
#define DBWORKER_H

#include <QObject>
#include <QString>

class QTimer;

/**
 * @brief 数据库写线程工作者（独立 QSqlDatabase 连接，不可跨线程共享）
 *        内置连接自愈：每次写前检查连接可用性，断线自动重连；并通过保活定时器周期探测，
 *        避免 MySQL wait_timeout 关闭空闲连接后长期处于失效状态。
 */
class DbWorker : public QObject
{
    Q_OBJECT
public:
    explicit DbWorker(QObject *parent = nullptr);
    ~DbWorker() override;

public slots:
    void openDatabase(const QString &host, int port, const QString &dbName,
                      const QString &user, const QString &password);
    void closeDatabase();
    void insertDataRecord(int slotNo, const QString &status, const QString &modelName,
                          const QString &modelCode, int count, const QString &currentTime,
                          const QString &recordSource, int plcStripeBatch);
    void executeSql(const QString &sql);
    void keepAlive(); ///< 保活探测：连接失效时自动重连

signals:
    void databaseOpened(bool ok, const QString &error);
    void writeFailed(const QString &error);
    void insertCompleted(); ///< 一条 data_records 插入成功（主线程据此再刷统计，避免异步写库竞态）

private:
    bool openConnectionInternal(); ///< 实际建连（调用前不应持有本连接的 QSqlDatabase 句柄）
    bool ensureConnected();        ///< 检查并修复连接（调用前不应持有本连接的 QSqlDatabase 句柄）
    static bool isConnectionLostError(const QString &error); ///< 判断是否为连接中断类错误

    QString m_connectionName;
    QString m_host;
    int m_port = 3306;
    QString m_dbName;
    QString m_user;
    QString m_password;
    QString m_lastOpenError;
    bool m_opened = false;
    QTimer *m_keepAliveTimer = nullptr;
};

#endif // DBWORKER_H
