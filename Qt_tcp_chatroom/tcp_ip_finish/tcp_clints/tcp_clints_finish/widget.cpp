#include "widget.h"
#include "ui_widget.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QHostAddress>

#include <QDialog>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QSqlQuery>
#include <QLabel>

void Widget::initDatabase()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("chat_history2.db");

    if (!db.open()) {
        qDebug() << "Error: connection with database failed";
    } else {
        qDebug() << "Database: connection ok";
    }

    QSqlQuery query;
    if (!query.exec("CREATE TABLE IF NOT EXISTS messages ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, "
                    "sender VARCHAR(255), "
                    "message TEXT, "
                    "ip VARCHAR(255))")) {
        qDebug() << "Error: failed to create messages table";
    }
}


Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    ui->setupUi(this);
    initDatabase();
    ui->connectBtn->setStyleSheet("background-color: rgb(6,163,220)");
    ui->hisBtn->setStyleSheet("background-color: rgb(6,163,220)");
    ui->sendBtn->setStyleSheet("background-color: rgb(6,163,220)");
    ui->leport->setStyleSheet("color:blue");
    ui->leipAddress->setStyleSheet("color:blue");

    ui->listWidget->setStyleSheet("border:2px solid blue");

    socket = new QTcpSocket(this);
    connectState = false;     //未连接状态

    messageSound = new QSound(":/new/prefix1/sounds/iphone.wav", this);
    connectSound = new QSound(":/new/prefix1/sounds/keke.wav", this);

    this->setWindowIcon(QIcon(":/new/prefix1/image/qq.png"));

    connect(socket, &QTcpSocket::readyRead, this, &Widget::readMessage);    //接收信息
    connect(socket, &QTcpSocket::disconnected, this, &Widget::disconnectSlot);   //打印断开连接信息
}

Widget::~Widget()
{
    delete ui;
}

void Widget::storeMessage(const QString &sender, const QString &message, const QString &ip)
{
    QSqlQuery query;
    query.prepare("INSERT INTO messages (sender, message, ip) VALUES (:sender, :message, :ip)");
    query.bindValue(":sender", sender);
    query.bindValue(":message", message);
    query.bindValue(":ip", ip);
    if (!query.exec()) {
        qDebug() << "Error inserting message into the database";
    }
}


void Widget::loadMessages()
{
    QSqlQuery query("SELECT timestamp, sender, message FROM messages ORDER BY timestamp DESC");
    while (query.next()) {
        QString timestamp = query.value(0).toString();
        QString sender = query.value(1).toString();
        QString message = query.value(2).toString();
        QString displayMessage = "[" + sender + "] " + timestamp + '\n' + message;
        ui->textReceive->append(displayMessage); // Assuming you have a text box for displaying messages
    }
}


void Widget::readMessage()    //接收信息
{
    messageSound->play();
    QByteArray arr = socket->readAll();
    QString str;
    QString name = "[Partner Message]";
    // 使用HTML格式设置文本颜色为蓝色
    str = "<span style=\"color:blue;\">" + name + QDateTime::currentDateTime().toString("dddd yyyy.MM.dd hh:mm:ss") + '\n' + QString(arr) + "</span>";
    ui->textReceive->append(str);     //以蓝色字体显示信息
    QString ip = socket->peerAddress().toString();

    // 保存消息并包括IP地址
    storeMessage(name, QString(arr), ip);
}



void Widget::disconnectSlot()    //打印连接断开信息
{
    ui->listWidget->addItem("clint disconnected");
}


void Widget::on_connectBtn_clicked()      //与客户端连接或者断开
{

    QString ipStr = ui->leipAddress->text();    //界面显示的地址
    quint16 currentPort = ui->leport->text().toInt();   //界面显示的当前端口
    if(!connectState)    //客户端还未连接服务端
    {
        socket->connectToHost(ipStr, currentPort);   //连接服务端
        if(socket->waitForConnected())   //等待连接成功
        {
            ui->listWidget->addItem("连接成功");
            ui->connectBtn->setText("关闭连接");
            connectSound->play();
            connectState = true;
        }

        else     //连接失败
        {
            QMessageBox::warning(this, "连接失败", socket->errorString());   //连接错误信息提醒
        }
    }

    else   //客户端已经连接
    {
        socket->close();   //关闭套接字，此时会发送disconnected信号
        connectSound->play();
        ui->connectBtn->setText("连接");
    }
}


void Widget::on_sendBtn_clicked()    //给服务端发送信息
{
    QString str = ui->textSend->toPlainText();
    if(socket->isOpen() && socket->isValid())
    {
        socket->write(str.toUtf8());    //给服务端发送信息
        ui->textSend->clear();
    }
    QString name = "[Your message]";
    QString showStr = name + QDateTime::currentDateTime().toString("dddd yyyy.MM.dd hh:mm:ss")  + '\n' + str;
    ui->textReceive->append(showStr);     //显示自己发送的信息
    QString ip = socket->peerAddress().toString();

    // 保存消息并包括IP地址
    storeMessage(name, str, ip);
}

void Widget::loadMessagesInto(QTextEdit* textEdit, const QString& ipFilter) {
    QSqlQuery query;
    query.prepare("SELECT timestamp, sender, message FROM messages WHERE ip = :ip ORDER BY timestamp DESC");
    query.bindValue(":ip", ipFilter);
    query.exec();

    bool hasMessages = false;
    while (query.next()) {
        hasMessages = true;
        QString timestamp = query.value(0).toString();
        QString sender = query.value(1).toString();
        QString message = query.value(2).toString();

        // 判断消息是否来自"[Partner Message]"
        if (sender == "[Partner Message]") {
            message = "<span style='color: blue;'>" + sender + " " + timestamp + "\n" + message + "</span>";
        } else {
            message = sender + " " + timestamp + "\n" + message;
        }
        textEdit->append(message);
    }

    if (!hasMessages) {
        textEdit->setPlainText("Empty");
    }
}




void Widget::on_hisBtn_clicked() {
    QDialog *historyDialog = new QDialog(this);
    historyDialog->setWindowTitle("历史消息");
    QVBoxLayout *layout = new QVBoxLayout(historyDialog);

    QTextEdit *textEdit = new QTextEdit();
    textEdit->setReadOnly(true);
    layout->addWidget(textEdit);

    QPushButton *clearButton = new QPushButton("清空历史记录");
    layout->addWidget(clearButton);
    connect(clearButton, &QPushButton::clicked, [this, textEdit, historyDialog]() {
        QSqlQuery query;
        if(query.exec("DELETE FROM messages")) {
            QMessageBox::information(historyDialog, "删除成功", "所有历史记录已被删除！");
            textEdit->clear();
        } else {
            QMessageBox::critical(historyDialog, "删除失败", "无法删除历史记录！");
        }
    });
    QString ip = socket->peerAddress().toString();
    loadMessagesInto(textEdit, ip);

    historyDialog->setLayout(layout);
    historyDialog->resize(400, 300);
    historyDialog->exec();
}






