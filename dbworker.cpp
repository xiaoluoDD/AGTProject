#include "dbworker.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QTimer>
#include <QUuid>
#include <QDebug>

DbWorker::DbWorker(QObject *parent)
    : QObject(parent)
    , m_connectionName(QStringLiteral("agt_db_worker_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
    , m_keepAliveTimer(new QTimer(this))
{
    // 15 秒保活探测一次，防止空闲连接被 MySQL wait_timeout 关闭
    m_keepAliveTimer->setInterval(15000);
    connect(m_keepAliveTimer, &QTimer::timeout, this, &DbWorker::keepAlive);
}

DbWorker::~DbWorker()
{
    closeDatabase();
}

bool DbWorker::openConnectionInternal()
{
    closeDatabase();

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QMYSQL"), m_connectionName);
    db.setHostName(m_host);
    db.setPort(m_port);
    db.setDatabaseName(m_dbName);
    db.setUserName(m_user);
    db.setPassword(m_password);
    db.setConnectOptions(QStringLiteral("MYSQL_OPT_CONNECT_TIMEOUT=3"));

    m_opened = db.open();
    if (!m_opened) {
        m_lastOpenError = db.lastError().text();
        qWarning() << "DbWorker open failed:" << m_lastOpenError;
    } else {
        m_lastOpenError.clear();
    }
    return m_opened;
}

bool DbWorker::ensureConnected()
{
    // 已连接且可用则直接返回
    if (m_opened && QSqlDatabase::contains(m_connectionName)) {
        {
            QSqlDatabase db = QSqlDatabase::database(m_connectionName);
            if (db.isOpen()) {
                QSqlQuery ping(db);
                if (ping.exec(QStringLiteral("SELECT 1"))) {
                    return true;
                }
                qWarning() << "DbWorker 连接失效，准备重连:" << ping.lastError().text();
            }
        } // 作用域结束，销毁 db/query 句柄后再重连，避免 removeDatabase 冲突
        m_opened = false;
    }
    return openConnectionInternal();
}

bool DbWorker::isConnectionLostError(const QString &error)
{
    const QString e = error.toLower();
    return e.contains(QStringLiteral("lost connection"))
        || e.contains(QStringLiteral("server has gone away"))
        || e.contains(QStringLiteral("connection refused"))
        || e.contains(QStringLiteral("connection closed"))
        || e.contains(QStringLiteral("not connected"))
        || e.contains(QStringLiteral("already closed"));
}

void DbWorker::openDatabase(const QString &host, int port, const QString &dbName,
                            const QString &user, const QString &password)
{
    m_host = host;
    m_port = port;
    m_dbName = dbName;
    m_user = user;
    m_password = password;

    const bool ok = openConnectionInternal();
    // 无论成败都启动保活定时器：DB 恢复后会自动重连
    m_keepAliveTimer->start();
    emit databaseOpened(ok, ok ? QString() : m_lastOpenError);
}

void DbWorker::closeDatabase()
{
    m_keepAliveTimer->stop();
    m_opened = false;
    if (!QSqlDatabase::contains(m_connectionName)) {
        return;
    }
    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName);
        if (db.isOpen()) {
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

void DbWorker::keepAlive()
{
    if (!ensureConnected()) {
        qWarning() << "DbWorker 保活重连失败:" << m_lastOpenError;
    }
}

void DbWorker::insertDataRecord(int slotNo, const QString &status, const QString &modelName,
                                const QString &modelCode, int count, const QString &currentTime,
                                const QString &recordSource, int plcStripeBatch)
{
    // 最多尝试 2 次：首次失败若为连接中断则重连后重试一次
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!ensureConnected()) {
            emit writeFailed(QStringLiteral("DbWorker 数据库未连接，重连失败: %1").arg(m_lastOpenError));
            return;
        }

        QSqlDatabase db = QSqlDatabase::database(m_connectionName);
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "INSERT INTO data_records (slot_no, status, model_name, model_code, count, time, record_source, plc_stripe_batch) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
        query.addBindValue(slotNo);
        query.addBindValue(status);
        query.addBindValue(modelName);
        query.addBindValue(modelCode);
        query.addBindValue(count);
        query.addBindValue(currentTime);
        query.addBindValue(recordSource.isEmpty() ? QStringLiteral("plc") : recordSource);
        query.addBindValue(plcStripeBatch >= 0 ? QVariant(plcStripeBatch) : QVariant());

        if (query.exec()) {
            emit insertCompleted();
            return;
        }

        const QString err = query.lastError().text();
        if (!isConnectionLostError(err)) {
            emit writeFailed(err);
            return;
        }
        // 连接在查询中途断开：循环结束时销毁句柄，下次循环重连后重试
        qWarning() << "DbWorker 查询中断，准备重连重试:" << err;
    }
    emit writeFailed(QStringLiteral("DbWorker 插入数据记录重试后仍失败"));
}

void DbWorker::executeSql(const QString &sql)
{
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!ensureConnected()) {
            emit writeFailed(QStringLiteral("DbWorker 数据库未连接，重连失败: %1").arg(m_lastOpenError));
            return;
        }

        QSqlDatabase db = QSqlDatabase::database(m_connectionName);
        QSqlQuery query(db);
        if (query.exec(sql)) {
            return;
        }

        const QString err = query.lastError().text();
        if (!isConnectionLostError(err)) {
            emit writeFailed(err);
            return;
        }
        qWarning() << "DbWorker SQL 执行中断，准备重连重试:" << err;
    }
    emit writeFailed(QStringLiteral("DbWorker SQL 执行重试后仍失败"));
}
