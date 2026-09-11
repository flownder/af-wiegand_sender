#include "mqtt_client.h"

#include <QDebug>
#include <QHostAddress>

mqtt_client::mqtt_client(QObject *parent)
    : QObject{parent}
{
}

mqtt_client::~mqtt_client()
{
    if (m_client) {
        disconnect(m_client, nullptr, this, nullptr);

        if (m_client->isConnectedToHost()) {
            m_client->disconnectFromHost();
        }

        delete m_client;
        m_client = nullptr;
    }
}

void mqtt_client::connectToBroker(const QString &host, quint16 port)
{
    // Если клиент уже был создан — корректно удаляем старый
    if (m_client) {
        disconnect(m_client, nullptr, this, nullptr);

        if (m_client->isConnectedToHost()) {
            m_client->disconnectFromHost();
        }

        m_client->deleteLater();
        m_client = nullptr;
    }

    const QHostAddress address(host);
    if (address.isNull()) {
        m_ConnectStatus = false;
        emit connectedEvent(false);
        emit connectionError(tr("Некорректный адрес брокера"));
        return;
    }

    m_client = new QMQTT::Client(address, port, this);

    m_client->setUsername("alcoframe");
    m_client->setPassword("alcostop");
    m_client->setKeepAlive(30);
    m_client->setCleanSession(true);
    m_client->setAutoReconnect(true);
    m_client->setAutoReconnectInterval(5);

    connect(m_client, &QMQTT::Client::connected,
            this, &mqtt_client::onConnected);

    connect(m_client, &QMQTT::Client::subscribed,
            this, &mqtt_client::onSubscribed);

    connect(m_client, &QMQTT::Client::disconnected,
            this, &mqtt_client::onDisconnected);

    connect(m_client, &QMQTT::Client::received,
            this, &mqtt_client::onReceived);

    connect(m_client, &QMQTT::Client::error,
            this, &mqtt_client::onError);

    m_client->connectToHost();
}

void mqtt_client::disconnectFromBroker()
{
    if (!m_client) {
        return;
    }

    disconnect(m_client, nullptr, this, nullptr);

    if (m_client->isConnectedToHost()) {
        m_client->disconnectFromHost();
    }

    m_client->deleteLater();
    m_client = nullptr;

    m_ConnectStatus = false;
    emit connectedEvent(false);
}

bool mqtt_client::isConnected() const
{
    return m_client && m_client->isConnectedToHost();
}

void mqtt_client::onConnected()
{
    qDebug() << "Connected to MQTT broker";

    m_ConnectStatus = true;
    emit connectedEvent(true);

    if (m_client) {
        m_client->subscribe("AlcoFrmDevice/WiegandBase/Out/Parameters/#", 0);
        m_client->subscribe("AlcoFrmDevice/WiegandBase/Out/Statuses/sLog", 0);
    }
}

void mqtt_client::onDisconnected()
{
    qDebug() << "onDisconnected";

    m_ConnectStatus = false;
    emit connectedEvent(false);
}

void mqtt_client::onSubscribed(const QString &topic)
{
    Q_UNUSED(topic);
}

void mqtt_client::onReceived(const QMQTT::Message &message)
{
    emit getMessage(message.topic(), QString::fromUtf8(message.payload()));
}

void mqtt_client::onError(const QMQTT::ClientError error)
{
    qDebug() << "MQTT error:" << static_cast<int>(error);

    m_ConnectStatus = false;
    emit connectedEvent(false);
    emit connectionError(tr("Ошибка MQTT (код %1)").arg(static_cast<int>(error)));
}

void mqtt_client::pubMess(const QString &topic, const QString &message)
{
    if (!isConnected()) {
        qDebug() << "MQTT publish skipped, not connected:" << topic;
        return;
    }

    QMQTT::Message mess(0, topic, message.toUtf8(), 0, false);
    m_client->publish(mess);
}
