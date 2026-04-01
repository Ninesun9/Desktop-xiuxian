#include "dialog.h"
#include "ui_dialog.h"
#include <QMovie>
#include <QtMath>
#include <QMessageBox>
#include "popupmanage.h"
#include "QFileDialog"
#include <QSettings>
#include <QDir>
#include <QMessageBox>

#include "core/playerprofilestore.h"
#include "network/playerstatsreporter.h"
#include "secure_transport.h"

Dialog::Dialog(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Dialog)
{
    ui->setupUi(this);
    setWindowTitle("透明无边框窗口");
    setWindowFlags(Qt::FramelessWindowHint|Qt::Tool|Qt::SubWindow|Qt::Popup|Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    _flags = windowFlags();
    _actionN5012346.setText("n5012346");
    _actionN5012346.setEnabled(false);
    _action52pojie.setText("52pojie");
    _action52pojie.setEnabled(false);
    _exit.setText("结束修炼");
    _deleteData.setText("自废修为");
    _rank.setText("排行榜");
    _switchSkin.setText("变身");
    _info.setText("修炼状态");
    _setTop.setText("置顶");
    _setTop.setCheckable(true);
    _setTop.setChecked(true);
    _chat.setText(QStringLiteral("Mail"));
    //pop_menu.addAction(&_action52pojie);
    //pop_menu.addAction(&_actionN5012346);
    pop_menu.addAction(&_setTop);
    pop_menu.addAction(&_info);
    pop_menu.addAction(&_chat);
    pop_menu.addAction(&_rank);
    pop_menu.addAction(&_switchSkin);
    pop_menu.addAction(&_exit);
    pop_menu.addAction(&_deleteData);
    pop_menu.addAction(ui->_AutoStart);
    pop_menu.addAction(ui->_changeName);
    pop_menu.addAction(ui->augur);
    pop_menu.addAction(ui->feedback);
    pop_menu.addAction(ui->jianghu);


    connect(&_exit,&QAction::triggered,[](bool){
                qApp->exit(0);
            });
    connect(&_deleteData,&QAction::triggered,[](bool){
                {
                    auto re = QMessageBox::information(NULL, "自废修为", "这将会删除全部数据确定操作吗？",
                                             QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                    if(re == QMessageBox::Yes)
                    {
                        {
                            auto re = QMessageBox::information(NULL, "再次确认", "真的真的真的要删档吗？",
                                                     QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                            if(re == QMessageBox::Yes)
                            {
                                QFile::remove(QDir::homePath()+"/AppData/Roaming/xiuxian.ini");
                                qApp->exit();
                            }
                            else
                            {
                                return;

                            }
                        }

                    }
                    else
                    {
                        return ;
                    }
                }

            });
    connect(&_rank,&QAction::triggered,this,&Dialog::showRankList);
    connect(&_switchSkin,&QAction::triggered,this,&Dialog::swicthSkin);
    connect(&_info,&QAction::triggered,[this](bool){
        QString info;
        info.append((this->_userName)+"\n");
        info.append("生命："+QString::number(_shengming.toDouble(),'f',5)+"\n");
        info.append("攻击："+QString::number(_gongji.toDouble(),'f',5)+"\n");
        info.append("防御："+QString::number(_fangyu.toDouble(),'f',5)+"\n");
        info.append("悟性："+QString::number(_wuxing.toDouble(),'f',5)+"\n");
        info.append("境界："+jieDuanNameList.at(_jingjie)+"\n");
        info.append("修为："+QString::number(_xiuwei.toDouble(),'f',5));
        QMessageBox::information(this,"修炼状态",info);
            });
    connect(&_setTop,&QAction::triggered,[this](bool){
            if(isToping)
            {
                setWindowFlags(Qt::FramelessWindowHint|Qt::Tool|Qt::SubWindow|Qt::Popup);
                show();
                _setTop.setText("置顶");
                _setTop.setChecked(false);
                isToping = false;
            }
            else
            {
                setWindowFlags(Qt::FramelessWindowHint|Qt::Tool|Qt::SubWindow|Qt::Popup|Qt::WindowStaysOnTopHint);
                show();
                _setTop.setText("置顶");
                _setTop.setChecked(true);
                isToping = true;
            }
            });
    connect(ui->_AutoStart,&QAction::triggered,[this](bool shate){
        QString application_name = "xiuxian";
           QSettings *settings = new QSettings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat );
       if(shate)
       {
           QString application_path = QApplication::applicationFilePath();
           settings->setValue(application_name, application_path.replace("/", "\\"));

       }
       else
       {
           settings->remove(application_name);
       }
       delete settings;
       this->ui->_AutoStart->setChecked(isAppAutoRun());

            });
    this->ui->_AutoStart->setChecked(isAppAutoRun());



    connect(ui->feedback,&QAction::triggered,[this](bool){
                this->_feedback.show();
            });
    connect(ui->jianghu,&QAction::triggered,[this](bool){
                _jianghu.show();
            });

    connect(&_chat,&QAction::triggered,this,&Dialog::toggleMailDrawer);
    jieDuanNameList.append("炼体期");
    jieDuanNameList.append("练气期");
    jieDuanNameList.append("筑基期");
    jieDuanNameList.append("金丹期");
    jieDuanNameList.append("元婴期");
    jieDuanNameList.append("化神期");
    jieDuanNameList.append("炼虚期");
    jieDuanNameList.append("合体期");
    jieDuanNameList.append("大乘期");
    jieDuanNameList.append("渡劫期");

    QMovie *movie = new QMovie(":/2021_08_21_12_06_56_979.gif");
    ui->label->setMovie(movie);
    movie->start();
    /**
     * @brief 初始化在这里
     */
    initThis();


    _jianghu.setInfo(_userName,_userid);
    _chatWindow.setInfo(_userName,_userid);
    _mailDrawer.setPlayerInfo(_userName, _userid);

    connect(&timer1s,&QTimer::timeout,this,&Dialog::everySecond);
    connect(&timer10s,&QTimer::timeout,this,&Dialog::every10Second);
    connect(&timer60s,&QTimer::timeout,this,&Dialog::every60Second);
    timer1s.start(1000);
    timer10s.start(10000);
    timer60s.start(30000);



    //QObject::connect(&w,&Dialog::sigChatShow,&chat,&ChatWindow::show);
}

Dialog::~Dialog()
{
    delete ui;
}
void Dialog::everySecond()
{
    addXiuwei();
    refDispaly();
}
void Dialog::every10Second()
{
    levelUp();
    saveData();
}


void Dialog::every60Second()
{
    PlayerStatsSnapshot snapshot;
    snapshot.userId = _userid;
    snapshot.userName = _userName;
    snapshot.shengming = _shengming.toDouble();
    snapshot.gongji = _gongji.toDouble();
    snapshot.fangyu = _fangyu.toDouble();
    snapshot.wuxing = _wuxing;
    snapshot.jingjie = _jingjie;
    snapshot.xiuwei = _xiuwei;

    QString errorMessage;
    if (!PlayerStatsReporter::sendUpdate(snapshot, &errorMessage))
    {
        qWarning() << errorMessage;
    }
}
void Dialog::initThis()
{
    PlayerProfile profile;
    if (!PlayerProfileStore::load(&profile))
    {
        registerThis();
        return;
    }

    _userid = profile.userId;
    _userName = profile.userName;
    _shengming = profile.shengming;
    _gongji = profile.gongji;
    _fangyu = profile.fangyu;
    _wuxing = profile.wuxing;
    _jingjie = profile.jingjie;
    _xiuwei = profile.xiuwei;

    if(!PlayerProfileStore::hasValidUserId(_userid))
    {
        registerThis();
    }
}
void Dialog::registerThis()
{
    bool isOK;
    QString name = MakeName::getName();
    QString text = QInputDialog::getText(NULL, "起名字",
                                                       "请输入你的名字",
                                                       QLineEdit::Normal,
                                                       name,
                                                       &isOK);
    if(!isOK || text.isEmpty())
    {
        qApp->exit(1);
    }
    else
    {
        const PlayerProfile profile = PlayerProfileStore::createNew(text);
        _userName = profile.userName;
        _userid = profile.userId;
        _shengming = profile.shengming;
        _gongji = profile.gongji;
        _fangyu = profile.fangyu;
        _wuxing = profile.wuxing;
        _jingjie = profile.jingjie;
        _xiuwei = profile.xiuwei;
        saveData();
    }
}
void Dialog::refDispaly()
{
    ui->name->setText(_userName);
    ui->shengming->setText(QString::number(_shengming.toDouble(),'e',3));
    ui->gongji->setText(QString::number(_gongji.toDouble(),'e',3));
    ui->fangyu->setText(QString::number(_fangyu.toDouble(),'e',3));
    ui->wuxing->setText(QString::number(_wuxing.toDouble(),'e',3));
    ui->jingjie->setText(jieDuanNameList.at(_jingjie));
    ui->xiuwei->setText(QString::number(_xiuwei.toDouble(),'e',3));
}
void Dialog::saveData()
{
    PlayerProfile profile;
    profile.userId = _userid;
    profile.userName = _userName;
    profile.shengming = _shengming;
    profile.gongji = _gongji;
    profile.fangyu = _fangyu;
    profile.wuxing = _wuxing;
    profile.jingjie = _jingjie;
    profile.xiuwei = _xiuwei;
    PlayerProfileStore::save(profile);
}

void Dialog::repositionMailDrawer()
{
    if (!_mailDrawer.isVisible())
    {
        return;
    }

    const QPoint topRight = mapToGlobal(rect().topRight());
    _mailDrawer.move(topRight.x() + 12, topRight.y() + 16);
}

void Dialog::toggleMailDrawer()
{
    if (_mailDrawer.isVisible())
    {
        _mailDrawer.hide();
        return;
    }

    _mailDrawer.setPlayerInfo(_userName, _userid);
    _mailDrawer.show();
    repositionMailDrawer();
    _mailDrawer.raise();
    _mailDrawer.activateWindow();
}
void Dialog::levelUp()
{

    for(int i = 0;i<10;i++)
    {
        if(_xiuwei.toDouble() >= qPow(100,i) && _jingjie <i)
        {
            _jingjie = i;
            qreal wuxingOld = _wuxing.toDouble();

            _wuxing = QString::number(MakeName::getWuxing(i),'g',15);

            qreal beilv = _wuxing.toDouble()/wuxingOld;
            _shengming = QString::number(_shengming.toDouble()*beilv,'g',15);
            _shengming = QString::number(MakeName::getRandData(_shengming.toDouble(),0.7),'g',15);
            _gongji = QString::number(_gongji.toDouble()*beilv,'g',15);
            _gongji = QString::number(MakeName::getRandData(_gongji.toDouble(),0.7),'g',15);
            _fangyu = QString::number(_fangyu.toDouble()*beilv,'g',15);
            _fangyu = QString::number(MakeName::getRandData(_fangyu.toDouble(),0.7),'g',15);
            PopupManage::getInstance()->setInfomation("恭喜", "成功晋升到"+jieDuanNameList.at(_jingjie));
        }
    }


}

void Dialog::addXiuwei()
{
    QTime current_time =QTime::currentTime();
    int hour = current_time.hour();//当前的小时

    //()<<"xia"<<_xiuwei;
    if(hour<2)
    {
        _xiuwei=QString::number(_xiuwei.toDouble()+_wuxing.toDouble()*2,'g',15);
    }
    else if(hour <4)
    {
        _xiuwei=QString::number(_xiuwei.toDouble()+_wuxing.toDouble()*5,'g',15);
    }
    else if(hour <6)
    {

        _xiuwei=QString::number(_xiuwei.toDouble()+_wuxing.toDouble()*2,'g',15);
    }
    else
    {
        _xiuwei=QString::number(_xiuwei.toDouble()+_wuxing.toDouble(),'g',15);
    }
    //qDebug()<<"shang"<<_xiuwei;

}
void Dialog::showRankList(bool)
{
    if(rank != nullptr)
    {
        rank->deleteLater();
    }
    rank = new RankList;
    rank->show();
}
void Dialog::swicthSkin(bool)
{


        QString fileName = QFileDialog::getOpenFileName(
            this,
            "选择一张图片",
            "C:/",
            "images(*.png *.jpeg *.jpg *.bmp *.gif);");
        if (fileName.isEmpty()) {
            return;
        }
        else {
            _skinPng.load(fileName);
            ui->label_skin->setPixmap(_skinPng);
            ui->label_skin->setScaledContents(true);
            ui->label_skin->show();
            this->setGeometry(x(),y(),_skinPng.width(),_skinPng.height());
            ui->label_skin->setGeometry(0,0,_skinPng.width(),_skinPng.height());
            ui->baseSkin->hide();

        }


}

void Dialog::wheelEvent(QWheelEvent*event)
{
    if (QGuiApplication::keyboardModifiers() == Qt::ControlModifier)
    {
        // Ctrl键盘被按下
        if(event->delta() > 0) //当滚轮向上滑，远离使用者
        {
                //ui->textEdit->zoomIn(); //进行放大，textEdit的光标和字体都放大
            //_skinPng=_skinPng.scaled(_skinPng.width()*1.03,_skinPng.height()*1.03, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            //ui->label_skin->setPixmap(_skinPng);
            this->setGeometry(x(),y(),width()*1.03,height()*1.03);
            ui->label_skin->setGeometry(0,0,width(),height());
        }
        else //当滚轮向下滑，靠近使用者
        {
                //ui->textEdit->zoomOut(); //进行缩小，textEdit的光标和字体都缩小
            //_skinPng=_skinPng.scaled(_skinPng.width()*0.97,_skinPng.height()*0.97, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            //ui->label_skin->setPixmap(_skinPng);
            this->setGeometry(x(),y(),width()*0.97,height()*0.97);
            ui->label_skin->setGeometry(0,0,width(),height());
        }

    }
}
bool Dialog::isAppAutoRun()
{
    //查看注册表是否已经写入程序名称
    QSettings *reg = new QSettings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::Registry64Format);
    QString strValue = reg->value("xiuxian").toString();	//获取注册表中“YourAppName”对应的路径值
    QString path = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());	//获取当前程序完整路径
    bool flag = (strValue == path);		//flag为true则表示注册表中已有此项

    return flag;
}


void Dialog::on__changeName_triggered()
{
    bool isOK;
    QString name = MakeName::getName();
    QString text = QInputDialog::getText(NULL, "起名字",
                                                       "请输入你的名字",
                                                       QLineEdit::Normal,
                                                       name,
                                                       &isOK);
    if(!isOK || text.isEmpty())
    {
        return;
    }
    else
    {
        _userName = text;
        _mailDrawer.setPlayerInfo(_userName, _userid);
    }
}


void Dialog::on_augur_triggered()
{
    _augurWindow.show();
}

