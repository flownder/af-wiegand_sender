#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <QObject>
#include <qmqtt.h>

class mqtt_client : public QObject
{
    Q_OBJECT

public:
    explicit mqtt_client(QObject *parent = nullptr);
    ~mqtt_client();

    void pubMess(const QString &topic, const QString &message);

    void connectToBroker(const QString &host, quint16 port = 1883);
    void disconnectFromBroker();
    bool isConnected() const;

private slots:
    void onConnected();
    void onDisconnected();
    void onSubscribed(const QString &topic);
    void onReceived(const QMQTT::Message &message);
    void onError(const QMQTT::ClientError error);

signals:
    void getMessage(const QString &topic, const QString &message);
    void connectedEvent(bool status);
    void connectionError(const QString &message);

private:
    QMQTT::Client *m_client = nullptr;
    bool m_ConnectStatus = false;
};

#endif // MQTT_CLIENT_H
