#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "serialworker.h"

#include <QMessageBox>
#include <QHostAddress>
#include <QTimer>
#include <QStatusBar>
#include <QDebug>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QMetaObject>
#include <QThread>
#include <QSerialPortInfo>

const QString MainWindow::TOPIC_PREFIX_PARAMS_OUT = QStringLiteral("AlcoFrmDevice/WiegandBase/Out/Parameters/");
const QString MainWindow::TOPIC_PREFIX_STATUS_OUT = QStringLiteral("AlcoFrmDevice/WiegandBase/Out/Statuses/");
const QString MainWindow::TOPIC_PREFIX_CMDS_IN    = QStringLiteral("AlcoFrmDevice/WiegandBase/In/Commands/");
const QString MainWindow::TOPIC_PREFIX_PARAMS_IN  = QStringLiteral("AlcoFrmDevice/WiegandBase/In/Parameters/");

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    mqttStatusLabel = new QLabel(tr("MQTT: не подключено"), this);
    statusBar()->addPermanentWidget(mqttStatusLabel);
    updateMqttStatus(false);

    connect(&mq, &mqtt_client::getMessage, this, &MainWindow::onMessageHandle);
    connect(&mq, &mqtt_client::connectedEvent, this, &MainWindow::onMqttConnectionStatus);
    connect(&mq, &mqtt_client::connectionError, this, &MainWindow::onMqttConnectionError);

    mqttConnectTimer = new QTimer(this);
    mqttConnectTimer->setSingleShot(true);
    mqttConnectTimer->setInterval(5000); // 5 секунд

    connect(mqttConnectTimer, &QTimer::timeout, this, [this]() {
        if (m_connectRequested && !m_isConnected) {
            m_connectRequested = false;
            updateMqttStatus(false);
            showConnectionWarning(tr("Таймаут подключения (5 секунд)."));
        }
    });

    setupParameterWidgets();
    setupSerial();

    // mq.connectToBroker("127.0.0.1", 1883);
}

MainWindow::~MainWindow()
{
    stopSerialThread(serialThread1, serialWorker1);
    stopSerialThread(serialThread2, serialWorker2);
    delete ui;
}

void MainWindow::setupParameterWidgets()
{
    // Находим QGroupBox, внутри которых находятся параметры
    QGroupBox *cardGroup = findChild<QGroupBox*>("card_group");
    QGroupBox *modeGroup = findChild<QGroupBox*>("mode_group");

    QList<QGroupBox*> parameterGroups;
    if (cardGroup) parameterGroups.append(cardGroup);
    if (modeGroup) parameterGroups.append(modeGroup);

    if (parameterGroups.isEmpty()) {
        qDebug() << "Warning: No parameter groups (card_group, mode_group) found";
        return;
    }

    // Собираем все виджеты из нужных групп
    QList<QLineEdit*> lineEdits;
    QList<QComboBox*> comboBoxes;
    QList<QCheckBox*> checkBoxes;

    for (QGroupBox *group : parameterGroups) {
        lineEdits.append(group->findChildren<QLineEdit*>());
        comboBoxes.append(group->findChildren<QComboBox*>());
        checkBoxes.append(group->findChildren<QCheckBox*>());
    }

    // Подключаем QLineEdit
    for (QLineEdit* lineEdit : lineEdits) {
        connect(lineEdit, &QLineEdit::returnPressed,
                this, &MainWindow::onParameterChanged);
        connect(lineEdit, &QLineEdit::editingFinished,
                this, &MainWindow::onParameterChanged);
    }

    // Подключаем QComboBox
    for (QComboBox* comboBox : comboBoxes) {
        QString objectName = comboBox->objectName();

        // Пропускаем комбобоксы, которые уже имеют свои отдельные обработчики
        if (objectName == "sCASModeVariant" ||
            objectName == "resultType" ||
            objectName == "sWgFrameParity" ||
            objectName == "sWgFrameLen") {
            continue;
        }

        connect(comboBox, &QComboBox::currentTextChanged,
                this, &MainWindow::onComboBoxChanged);
    }

    // Подключаем QCheckBox
    for (QCheckBox* checkBox : checkBoxes) {
        QString objectName = checkBox->objectName();

        // Подключаем только реле с универсальным обработчиком
        if (objectName == "sRelay_1" || objectName == "sRelay_2") {
            connect(checkBox, &QCheckBox::clicked,
                    this, &MainWindow::onCheckBoxChanged);
        }
    }
}

void MainWindow::onParameterChanged()
{
    QLineEdit* lineEdit = qobject_cast<QLineEdit*>(sender());
    if (!lineEdit) return;

    QString paramName = lineEdit->objectName();
    if (paramName.isEmpty()) return;

    QString value = lineEdit->text();
    publishParameter(paramName, value);
}

void MainWindow::onComboBoxChanged(const QString &text)
{
    QComboBox* comboBox = qobject_cast<QComboBox*>(sender());
    if (!comboBox) return;

    QString paramName = comboBox->objectName();
    if (paramName.isEmpty()) return;

    publishParameter(paramName, text);
}

void MainWindow::onCheckBoxChanged(bool checked)
{
    QCheckBox* checkBox = qobject_cast<QCheckBox*>(sender());
    if (!checkBox) return;

    QString paramName = checkBox->objectName();
    if (paramName.isEmpty()) return;

    // Для реле используем полное имя параметра с "/inverted"
    QString fullParamName;
    if (paramName == "sRelay_1") {
        fullParamName = "sRelay_1/inverted";
    } else if (paramName == "sRelay_2") {
        fullParamName = "sRelay_2/inverted";
    } else {
        fullParamName = paramName;
    }

    QString value = checked ? "true" : "false";
    publishParameter(fullParamName, value);
}

void MainWindow::publishParameter(const QString &paramName, const QString &value)
{
    if (paramName.isEmpty() || value.isEmpty()) return;

    QString topic = "AlcoFrmDevice/WiegandBase/In/Parameters/" + paramName;
    mq.pubMess(topic, value);
    qDebug() << "Published:" << topic << "=" << value;
}

QString MainWindow::extractSuffix(const QString &topic, const QString &prefix)
{
    if (topic.startsWith(prefix)) {
        return topic.mid(prefix.length());
    }
    return QString();
}

void MainWindow::updateComboBoxByValue(QComboBox* comboBox, const QString &value, const QString &paramName)
{
    comboBox->blockSignals(true);

    // Специальная обработка для sCASModeVariant
    if (paramName == "sCASModeVariant") {
        bool ok;
        int intValue = value.toInt(&ok);
        if (ok && intValue >= 1 && intValue <= 3) {
            comboBox->setCurrentIndex(intValue - 1);
            qDebug() << "Updated QComboBox" << paramName << "with value:" << value << "(index:" << (intValue - 1) << ")";
        } else {
            qDebug() << "Invalid value for sCASModeVariant:" << value;
        }
    }
    // Специальная обработка для sResultType
    else if (paramName == "sResultType") {
        int index = comboBox->findText(value);
        if (index >= 0) {
            comboBox->setCurrentIndex(index);
            qDebug() << "Updated QComboBox" << paramName << "with value:" << value;
        } else {
            qDebug() << "Value" << value << "not found in QComboBox" << paramName;
        }
    }
    // Общая обработка для всех остальных QComboBox
    else {
        int index = comboBox->findText(value);
        if (index >= 0) {
            comboBox->setCurrentIndex(index);
            qDebug() << "Updated QComboBox" << paramName << "with value:" << value;
        } else {
            qDebug() << "Value" << value << "not found in QComboBox" << paramName;
        }
    }

    comboBox->blockSignals(false);
}

void MainWindow::onMessageHandle(const QString &topic, const QString &message)
{
    // 1. Параметры устройства -> обновляем виджеты
    QString paramName = extractSuffix(topic, TOPIC_PREFIX_PARAMS_OUT);
    if (!paramName.isEmpty()) {
        handleParameterMessage(paramName, message);
        return;
    }

    // 2. Статусы устройства -> обновляем индикаторы/логи
    QString statusName = extractSuffix(topic, TOPIC_PREFIX_STATUS_OUT);
    if (!statusName.isEmpty()) {
        handleStatusMessage(statusName, message);
        return;
    }

    // 3. Эхо команд (если брокер их возвращает)
    QString cmdName = extractSuffix(topic, TOPIC_PREFIX_CMDS_IN);
    if (!cmdName.isEmpty()) {
        handleCommandMessage(cmdName, message);
        return;
    }

    // 4. Неизвестный топик
    qDebug() << "Unknown topic:" << topic;
}

void MainWindow::handleParameterMessage(const QString &paramName, const QString &message)
{
    // Ищем QLineEdit с таким же objectName
    QLineEdit* lineEdit = findChild<QLineEdit*>(paramName);
    if (lineEdit) {
        lineEdit->blockSignals(true);
        lineEdit->setText(message);
        lineEdit->blockSignals(false);
        qDebug() << "Updated QLineEdit" << paramName << "=" << message;
        return;
    }

    // Ищем QComboBox
    QComboBox* comboBox = findChild<QComboBox*>(paramName);
    if (comboBox) {
        updateComboBoxByValue(comboBox, message, paramName);
        return;
    }

    // QCheckBox реле
    QString checkBoxName;
    if (paramName == "sRelay_1/inverted") {
        checkBoxName = "sRelay_1";
    } else if (paramName == "sRelay_2/inverted") {
        checkBoxName = "sRelay_2";
    } else {
        checkBoxName = paramName;
    }

    QCheckBox* checkBox = findChild<QCheckBox*>(checkBoxName);
    if (checkBox) {
        checkBox->blockSignals(true);
        checkBox->setChecked(message == "true" || message == "1");
        checkBox->blockSignals(false);
        qDebug() << "Updated QCheckBox" << checkBoxName << "=" << message;
        return;
    }

    qDebug() << "No widget for parameter:" << paramName;
}


void MainWindow::handleStatusMessage(const QString &statusName, const QString &message)
{
    if (statusName == "sLog")
        ui->logViewer->append(message);
/*
    // Пример 1: общий статус устройства
    if (statusName == "connection" || statusName == "deviceState") {
        // Можно обновить QLabel на форме, например ui->labelDeviceStatus
        // ui->labelDeviceStatus->setText(message);
        statusBar()->showMessage(tr("Device: %1").arg(message), 3000);
        return;
    }

    // Пример 2: статус считывателя карты
    if (statusName == "cardReader") {
        // Допустим, message = "ready" / "reading" / "error"
        if (message == "error") {
            statusBar()->showMessage(tr("Card reader ERROR"), 5000);
        }
        return;
    }

    // Пример 3: последний считанный UID карты
    if (statusName == "lastCardUid") {
        // Можно записать в QLineEdit, QLabel или textEdit
        QLineEdit* uidEdit = findChild<QLineEdit*>("lastCardUid");
        if (uidEdit) {
            uidEdit->blockSignals(true);
            uidEdit->setText(message);
            uidEdit->blockSignals(false);
        }
        return;
    }

    // Пример 4: счётчик событий
    if (statusName == "eventCounter") {
        QLabel* counterLabel = findChild<QLabel*>("eventCounterLabel");
        if (counterLabel) {
            counterLabel->setText(message);
        }
        return;
    }

    qDebug() << "Unhandled status:" << statusName;
*/
}

void MainWindow::handleCommandMessage(const QString &commandName, const QString &message)
{
    qDebug() << "Command echo:" << commandName << "=" << message;

    // Например, логируем в текстовое поле
    // ui->textEditLog->append(tr("CMD %1: %2").arg(commandName, message));
}

// Остальные методы без изменений
void MainWindow::on_pushButton_clicked()
{
    const QString &channel  = ui->channel->currentText();
    const QString &volume   = QLocale::c().toString(ui->volume->value(), 'f', ui->volume->decimals());
    const QString &units    = ui->units->currentText();
    const QString &status   = ui->status->currentText();

    const QString result = "channel_number=" + channel + ", result=" + volume + ", units=" + units + ", status=" + status;

    mq.pubMess("AlcoFrmDevice/WiegandBase/In/Commands/sTranslateWiegandPromilles", result);
}

void MainWindow::on_sWgFrameParity_currentTextChanged(const QString &arg1)
{
    mq.pubMess("AlcoFrmDevice/WiegandBase/In/Parameters/sWgFrameParity", arg1);
}

void MainWindow::on_sWgFrameLen_currentTextChanged(const QString &arg1)
{
    mq.pubMess("AlcoFrmDevice/WiegandBase/In/Parameters/sWgFrameLen", arg1);
}

void MainWindow::on_pushRelay_1_clicked()
{
    mq.pubMess("AlcoFrmDevice/WiegandBase/In/Commands/sRelay", "relay_id=1, duration=" + ui->relay_1_duration->text());
}

void MainWindow::on_pushRelay_2_clicked()
{
    mq.pubMess("AlcoFrmDevice/WiegandBase/In/Commands/sRelay", "relay_id=2, duration=" + ui->relay_2_duration->text());
}

void MainWindow::on_sCASModeVariant_activated(int index)
{
    QString value;
    switch (index) {
    case 0: value = "1"; break;
    case 1: value = "2"; break;
    case 2: value = "3"; break;
    default: return;
    }
    mq.pubMess("AlcoFrmDevice/WiegandBase/In/Parameters/sCASModeVariant", value);
}


void MainWindow::on_sResultType_activated(int index)
{
    switch (index) {
    case 0: mq.pubMess("AlcoFrmDevice/WiegandBase/In/Parameters/sResultType", "quantity");break;
    case 1: mq.pubMess("AlcoFrmDevice/WiegandBase/In/Parameters/sResultType", "quality"); break;
    default:
        break;
    }
}


void MainWindow::on_pushButton_2_clicked()
{
    const QString &channel = ui->channel_number->currentText();
    const QString &event   = ui->event->currentText();

    const QString result = "channel_number=" + channel + ", event=" + event;

    mq.pubMess("AlcoFrmDevice/WiegandBase/In/Commands/sWiegandEvent", result);
}


void MainWindow::on_Connect_pushButton_clicked()
{
    // QLineEdit *ipEdit = findChild<QLineEdit*>(QStringLiteral("ip_lineEdit"));

    // if (!ipEdit) {
    //     QMessageBox::warning(this, tr("MQTT"),
    //                          tr("Не найдено поле ввода с именем ip_lineEdit."));
    //     return;
    // }

    if (m_connectRequested) {
        qDebug() << "Connection already in progress";
        return;
    }

    if (m_isConnected || mq.isConnected()) {
        QMessageBox::information(this, tr("MQTT"),
                                 tr("Уже подключено к MQTT брокеру."));
        return;
    }

    const QString ip = ui->ip_lineEdit->text().trimmed();

    if (ip.isEmpty()) {
        QMessageBox::warning(this, tr("MQTT"),
                             tr("Введите IP-адрес MQTT брокера."));
        return;
    }

    const QHostAddress addr(ip);

    if (addr.isNull()) {
        QMessageBox::warning(this, tr("MQTT"),
                             tr("Некорректный IP-адрес: %1").arg(ip));
        return;
    }

    m_connectRequested = true;
    m_warningShown = false;
    m_isConnected = false;

    updateMqttStatus(false, tr("подключение..."));

    mq.connectToBroker(ip, 1883);

    // Запускаем таймер только если подключение ещё не завершилось успешно/ошибкой
    if (m_connectRequested && mqttConnectTimer) {
        mqttConnectTimer->start();
    }
}

void MainWindow::onMqttConnectionStatus(bool connected)
{
    m_isConnected = connected;

    if (connected) {
        m_connectRequested = false;
        m_warningShown = false;

        if (mqttConnectTimer) {
            mqttConnectTimer->stop();
        }
    }

    updateMqttStatus(connected);
}

void MainWindow::onMqttConnectionError(const QString &message)
{
    m_isConnected = false;
    updateMqttStatus(false);

    if (m_connectRequested) {
        m_connectRequested = false;

        if (mqttConnectTimer) {
            mqttConnectTimer->stop();
        }

        showConnectionWarning(message);
    }
}

void MainWindow::updateMqttStatus(bool connected, const QString &stateText)
{
    if (!mqttStatusLabel) {
        return;
    }

    if (!stateText.isEmpty()) {
        mqttStatusLabel->setText(tr("MQTT: %1").arg(stateText));
        mqttStatusLabel->setStyleSheet(QStringLiteral(
            "color: #C77700; font-weight: bold;"
            ));
        return;
    }

    if (connected) {
        mqttStatusLabel->setText(tr("MQTT: подключено"));
        mqttStatusLabel->setStyleSheet(QStringLiteral(
            "color: green; font-weight: bold;"
            ));
    } else {
        mqttStatusLabel->setText(tr("MQTT: не подключено"));
        mqttStatusLabel->setStyleSheet(QStringLiteral(
            "color: red; font-weight: bold;"
            ));
    }
}

void MainWindow::showConnectionWarning(const QString &details)
{
    if (m_warningShown) {
        return;
    }

    m_warningShown = true;

    QString text = tr("Не удалось подключиться к MQTT брокеру.");

    if (!details.isEmpty()) {
        text += QStringLiteral("\n\n") + details;
    }

    QMessageBox::warning(this, tr("MQTT"), text);
}


void MainWindow::setupSerial()
{
    // Создаём два экземпляра класса работы с портом
    serialWorker1 = new SerialWorker();
    serialWorker2 = new SerialWorker();

    // Создаём два потока
    serialThread1 = new QThread(this);
    serialThread2 = new QThread(this);

    // Переносим объекты в потоки
    serialWorker1->moveToThread(serialThread1);
    serialWorker2->moveToThread(serialThread2);

    // После завершения потока удаляем worker
    connect(serialThread1, &QThread::finished,
            serialWorker1, &QObject::deleteLater);

    connect(serialThread2, &QThread::finished,
            serialWorker2, &QObject::deleteLater);

    // Приём данных от первого serial worker -> textEdit_1
    connect(serialWorker1, &SerialWorker::dataReceived,
            this, [this](const QByteArray &data) {

                if (!ui->textEdit_1) {
                    return;
                }

                ui->textEdit_1->moveCursor(QTextCursor::End);
                ui->textEdit_1->insertPlainText(QString::fromUtf8(data));
                ui->textEdit_1->ensureCursorVisible();

                // Если нужен вывод в HEX, закомментируй строки выше
                // и используй такой вариант:
                //
                // ui->textEdit_1->moveCursor(QTextCursor::End);
                // ui->textEdit_1->insertPlainText(QString::fromLatin1(data.toHex()) + ' ');
                // ui->textEdit_1->ensureCursorVisible();
            });

    // Приём данных от второго serial worker -> textEdit_2
    connect(serialWorker2, &SerialWorker::dataReceived,
            this, [this](const QByteArray &data) {

                if (!ui->textEdit_2) {
                    return;
                }

                ui->textEdit_2->moveCursor(QTextCursor::End);
                ui->textEdit_2->insertPlainText(QString::fromUtf8(data));
                ui->textEdit_2->ensureCursorVisible();

                // Если нужен вывод в HEX, закомментируй строки выше
                // и используй такой вариант:
                //
                // ui->textEdit_2->moveCursor(QTextCursor::End);
                // ui->textEdit_2->insertPlainText(QString::fromLatin1(data.toHex()) + ' ');
                // ui->textEdit_2->ensureCursorVisible();
            });

    // Ошибки первого порта выводим в textEdit_1
    connect(serialWorker1, &SerialWorker::errorOccurred,
            this, [this](const QString &errorText) {

                if (ui->textEdit_1) {
                    ui->textEdit_1->append(
                        QStringLiteral("<font color=\"red\">%1</font>")
                            .arg(errorText.toHtmlEscaped())
                        );
                }
            });

    // Ошибки второго порта выводим в textEdit_2
    connect(serialWorker2, &SerialWorker::errorOccurred,
            this, [this](const QString &errorText) {

                if (ui->textEdit_2) {
                    ui->textEdit_2->append(
                        QStringLiteral("<font color=\"red\">%1</font>")
                            .arg(errorText.toHtmlEscaped())
                        );
                }
            });

    // Запускаем потоки
    serialThread1->start();
    serialThread2->start();

    // Заполняем списки портов
    fillPortsCombo(ui->port_comboBox_1);
    fillPortsCombo(ui->port_comboBox_2);

    // Выбор порта для первого экземпляра
    connect(ui->port_comboBox_1, QOverload<int>::of(&QComboBox::activated),
            this, [this](int index) {

                if (index <= 0) {
                    return;
                }

                const QString portName =
                    ui->port_comboBox_1->itemData(index).toString();

                if (portName.isEmpty()) {
                    return;
                }

                // Вызываем слот уже в рабочем потоке
                QMetaObject::invokeMethod(
                    serialWorker1,
                    "openPort",
                    Qt::QueuedConnection,
                    Q_ARG(QString, portName)
                    );
            });

    // Выбор порта для второго экземпляра
    connect(ui->port_comboBox_2, QOverload<int>::of(&QComboBox::activated),
            this, [this](int index) {

                if (index <= 0) {
                    return;
                }

                const QString portName =
                    ui->port_comboBox_2->itemData(index).toString();

                if (portName.isEmpty()) {
                    return;
                }

                // Вызываем слот уже в рабочем потоке
                QMetaObject::invokeMethod(
                    serialWorker2,
                    "openPort",
                    Qt::QueuedConnection,
                    Q_ARG(QString, portName)
                    );
            });
}

void MainWindow::fillPortsCombo(QComboBox *combo)
{
    if (!combo) {
        return;
    }

    combo->blockSignals(true);
    combo->clear();

    combo->addItem(tr("Выберите порт"), QString());

    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();

    for (const QSerialPortInfo &info : ports) {
        QString visibleText = info.portName();

        if (!info.description().isEmpty()) {
            visibleText += QStringLiteral(" (%1)").arg(info.description());
        }

        // В data сохраняем системный путь к порту.
        // Например, в Linux это может быть /dev/ttyUSB0
        // В Windows обычно что-то вроде COM3
        combo->addItem(visibleText, info.systemLocation());
    }

    combo->blockSignals(false);
}

void MainWindow::stopSerialThread(QThread *thread, SerialWorker *worker)
{
    if (!thread) {
        return;
    }

    // Если поток ещё работает, сначала закрываем порт внутри рабочего потока
    if (thread->isRunning() && worker) {
        QMetaObject::invokeMethod(
            worker,
            "closePort",
            Qt::BlockingQueuedConnection
            );
    }

    thread->quit();
    thread->wait();
}

void MainWindow::on_pushGPIO_1_clicked()
{
    mq.pubMess("AlcoFrmDevice/WiegandBase/In/Commands/sGPIO", "gpio_id=1, duration=" + ui->gpio_1_duration->text());
}


void MainWindow::on_pushGPIO_2_clicked()
{
    mq.pubMess("AlcoFrmDevice/WiegandBase/In/Commands/sGPIO", "gpio_id=2, duration=" + ui->gpio_2_duration->text());
}

