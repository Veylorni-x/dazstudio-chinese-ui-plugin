// SPDX-License-Identifier: MIT
// Version-pinned, in-process UI paint adapter for Daz's custom-drawn controls.
// It never changes DzProperty/DzNode values or paints into files/images/printers.
#include <QPainter>
#include <QPaintEngine>
#include <QStaticText>
#include <QThread>
#include <QScopedValueRollback>
#include <QCryptographicHash>
#include <atomic>
#include "third_party/minhook/include/MinHook.h"

namespace PaintAdapter {
static std::atomic<Dictionary*> dictionary{nullptr};
static thread_local bool inside=false;
static thread_local bool settingText=false;
static bool installed=false;
static QList<void*> addresses;
static thread_local const QWidget*styleWidget=nullptr;
static thread_local bool inCameraCubePaint=false;
struct Guard{bool old;Guard():old(inside){inside=true;}~Guard(){inside=old;}};
struct StyleScope{const QWidget*old;StyleScope(const QWidget*w):old(styleWidget){if(w)styleWidget=w;}~StyleScope(){styleWidget=old;}};
struct CameraCubeScope{bool old;CameraCubeScope():old(inCameraCubePaint){inCameraCubePaint=true;}~CameraCubeScope(){inCameraCubePaint=old;}};

static QString translated(QPainter*p,const QString&s){
    auto*d=dictionary.load(std::memory_order_acquire);
    if(inside||!d||!d->enabled||s.isEmpty()||!p||!p->device())return s;
    if(!qApp||QThread::currentThread()!=qApp->thread())return s;
    static const QRegularExpression latin("[A-Za-z]");if(!s.contains(latin))return s;
    if(inCameraCubePaint)return d->display(s,"ComboDisplay");
    int deviceType=p->device()->devType();
    auto*w=styleWidget;
    if(!w&&deviceType==QInternal::Widget)w=dynamic_cast<QWidget*>(p->device());
    if(!w)return s;
    for(auto*parent=w;parent;parent=parent->parentWidget()){
        if(qobject_cast<const QLineEdit*>(parent)||qobject_cast<const QAbstractSpinBox*>(parent))return s;
        if(auto*t=qobject_cast<const QTextEdit*>(parent))if(!t->isReadOnly())return s;
        if(auto*t=qobject_cast<const QPlainTextEdit*>(parent))if(!t->isReadOnly())return s;
        QString id=QString::fromLatin1(parent->metaObject()->className())+' '+parent->objectName();
        static const QRegularExpression excluded("Script.*(?:Edit|View)|CodeEdit|CodeView|Console|Log(?:View|Edit|Text|Browser)|SourceEditor",QRegularExpression::CaseInsensitiveOption);
        if(id.contains(excluded))return s;
    }
    return d->display(s,QString("Paint:")+QString::fromLatin1(w->metaObject()->className()));
}
using Point=void(__fastcall*)(QPainter*,const QPointF&,const QString&);
using PointFlags=void(__fastcall*)(QPainter*,const QPointF&,const QString&,int,int);
using RectF=void(__fastcall*)(QPainter*,const QRectF&,int,const QString&,QRectF*);
using Rect=void(__fastcall*)(QPainter*,const QRect&,int,const QString&,QRect*);
using Option=void(__fastcall*)(QPainter*,const QRectF&,const QString&,const QTextOption&);
using Static=void(__fastcall*)(QPainter*,const QPointF&,const QStaticText&);
using Item=void(__fastcall*)(QPainter*,const QPointF&,const QTextItem&);
using Control=void(__fastcall*)(const void*,QStyle::ControlElement,const QStyleOption*,QPainter*,const QWidget*);
using Primitive=void(__fastcall*)(const void*,QStyle::PrimitiveElement,const QStyleOption*,QPainter*,const QWidget*);
using CubePaint=void(__fastcall*)(void*,QPainter*);
using Title=void(__fastcall*)(QWidget*,const QString&);
using Message=void(__fastcall*)(QMessageBox*,const QString&);
static Point realPoint=nullptr;static PointFlags realPointFlags=nullptr;
static RectF realRectF=nullptr;static Rect realRect=nullptr;static Option realOption=nullptr;
static Static realStatic=nullptr;static Item realItem=nullptr;
static Control realControl=nullptr;static Primitive realPrimitive=nullptr;static CubePaint realCubePaint=nullptr;
static Title realTitle=nullptr;static Message realMessage=nullptr;static Message realInformation=nullptr;
static void __fastcall cubePaint(void*self,QPainter*p){CameraCubeScope scope;realCubePaint(self,p);}
static QString dialogText(Dictionary*d,const QString&s){if(!d||!d->nativeVisuals)return s;auto t=d->lookup(s,"NativeDialog");return t.isNull()?s:t;}
static void __fastcall title(QWidget*w,const QString&s){
    auto*d=dictionary.load(std::memory_order_acquire);
    if(settingText||!d||!w||!qApp||QThread::currentThread()!=qApp->thread()||!qobject_cast<const QDialog*>(w)){realTitle(w,s);return;}
    QScopedValueRollback<bool>guard(settingText,true);d->rememberUi(w,"windowTitle",s);realTitle(w,dialogText(d,s));
}
static void __fastcall message(QMessageBox*w,const QString&s){
    auto*d=dictionary.load(std::memory_order_acquire);if(settingText||!d||!w||!qApp||QThread::currentThread()!=qApp->thread()){realMessage(w,s);return;}
    QScopedValueRollback<bool>guard(settingText,true);d->rememberUi(w,"message",s);realMessage(w,dialogText(d,s));
}
static void __fastcall information(QMessageBox*w,const QString&s){
    auto*d=dictionary.load(std::memory_order_acquire);if(settingText||!d||!w||!qApp||QThread::currentThread()!=qApp->thread()){realInformation(w,s);return;}
    QScopedValueRollback<bool>guard(settingText,true);d->rememberUi(w,"information",s);realInformation(w,dialogText(d,s));
}
static void refreshUi(Dictionary*d){
    if(!d||!installed)return;
    QList<QPair<QPointer<QObject>,QHash<QString,QString>>>snapshot;
    {
        QMutexLocker lock(&d->mutex);
        for(auto it=d->nativeUiOriginals.cbegin();it!=d->nativeUiOriginals.cend();++it)
            snapshot.append({QPointer<QObject>(it.key()),it.value()});
    }
    QScopedValueRollback<bool>guard(settingText,true);
    for(const auto&entry:snapshot){
        QPointer<QObject>object=entry.first;const auto&text=entry.second;
        if(!object)continue;
        if(auto*w=qobject_cast<QDialog*>(object.data()))if(realTitle&&text.contains("windowTitle"))realTitle(w,dialogText(d,text.value("windowTitle")));
        if(!object)continue;
        if(auto*w=qobject_cast<QMessageBox*>(object.data())){
            if(realMessage&&text.contains("message"))realMessage(w,dialogText(d,text.value("message")));
            if(object&&realInformation&&text.contains("information"))realInformation(w,dialogText(d,text.value("information")));
        }
    }
}
template<class T>static const T*exactOption(const QStyleOption*option){return option&&option->type==T::Type&&option->version==T::Version?static_cast<const T*>(option):nullptr;}
static void __fastcall control(const void*self,QStyle::ControlElement element,const QStyleOption*option,QPainter*p,const QWidget*w){
    StyleScope scope(w);
    if(element==QStyle::CE_ItemViewItem){if(auto*o=exactOption<QStyleOptionViewItem>(option)){
        QStyleOptionViewItem copy(*o);copy.text=translated(p,copy.text);Guard g;realControl(self,element,&copy,p,w);return;}}
    if(element==QStyle::CE_Header||element==QStyle::CE_HeaderLabel){if(auto*o=exactOption<QStyleOptionHeader>(option)){
        QStyleOptionHeader copy(*o);copy.text=translated(p,copy.text);Guard g;realControl(self,element,&copy,p,w);return;}}
    if(element==QStyle::CE_ComboBoxLabel){if(auto*o=exactOption<QStyleOptionComboBox>(option)){
        QStyleOptionComboBox copy(*o);auto*combo=qobject_cast<const QComboBox*>(w);
        if(!combo||!combo->isEditable())copy.currentText=translated(p,copy.currentText);Guard g;realControl(self,element,&copy,p,w);return;}}
    if(element==QStyle::CE_MenuItem||element==QStyle::CE_MenuBarItem){if(auto*o=exactOption<QStyleOptionMenuItem>(option)){
        QStyleOptionMenuItem copy(*o);int tab=copy.text.indexOf('\t');
        copy.text=tab<0?translated(p,copy.text):translated(p,copy.text.left(tab))+copy.text.mid(tab);Guard g;realControl(self,element,&copy,p,w);return;}}
    if(element==QStyle::CE_PushButtonLabel||element==QStyle::CE_CheckBoxLabel||element==QStyle::CE_RadioButtonLabel){if(auto*o=exactOption<QStyleOptionButton>(option)){
        QStyleOptionButton copy(*o);copy.text=translated(p,copy.text);Guard g;realControl(self,element,&copy,p,w);return;}}
    if(element==QStyle::CE_ToolButtonLabel){if(auto*o=exactOption<QStyleOptionToolButton>(option)){
        QStyleOptionToolButton copy(*o);copy.text=translated(p,copy.text);Guard g;realControl(self,element,&copy,p,w);return;}}
    if(element==QStyle::CE_TabBarTabLabel){if(auto*o=exactOption<QStyleOptionTab>(option)){
        QStyleOptionTab copy(*o);copy.text=translated(p,copy.text);Guard g;realControl(self,element,&copy,p,w);return;}}
    realControl(self,element,option,p,w);
}
static void __fastcall primitive(const void*self,QStyle::PrimitiveElement element,const QStyleOption*option,QPainter*p,const QWidget*w){
    StyleScope scope(w);realPrimitive(self,element,option,p,w);
    if(inCameraCubePaint&&option&&p&&p->device()&&int(element)>=0x0F00002B&&int(element)<=0x0F000030){
        auto*d=dictionary.load(std::memory_order_acquire);
        if(d&&d->enabled&&qApp&&QThread::currentThread()==qApp->thread()&&realRect){
            const char*src=nullptr;const char*fallbackZh=nullptr;
            switch(int(element)){
                case 0x0F00002B:src="Front";fallbackZh="正面";break;
                case 0x0F00002C:src="Back";fallbackZh="背面";break;
                case 0x0F00002D:src="Top";fallbackZh="顶面";break;
                case 0x0F00002E:src="Bottom";fallbackZh="底面";break;
                case 0x0F00002F:src="Left";fallbackZh="左侧";break;
                case 0x0F000030:src="Right";fallbackZh="右侧";break;
            }
            if(src&&fallbackZh){
                QString srcStr=QString::fromLatin1(src);
                QString label=d->contexts.value(QStringLiteral("DzCameraCube\x04")+srcStr);
                if(label.isEmpty())label=QString::fromUtf8(fallbackZh);
                QRect r=option->rect;
                if(r.width()>12&&r.height()>12){
                    QColor bg(114,114,118,255),fg(22,22,25,255);
                    QRect inner=r.adjusted(qMax(3,r.width()/12),qMax(4,r.height()/5),-qMax(3,r.width()/12),-qMax(4,r.height()/5));
                    if(auto*img=dynamic_cast<QImage*>(p->device())){
                        int sx=qBound(0,r.center().x(),img->width()-1);
                        int sy=qBound(0,r.top()+qMax(4,r.height()/6),img->height()-1);
                        bg=img->pixelColor(sx,sy);
                        int bestDiff=-1;QRect scan=inner.intersected(img->rect());
                        for(int y=scan.top();y<=scan.bottom();y+=2)for(int x=scan.left();x<=scan.right();x+=2){
                            QColor c=img->pixelColor(x,y);if(c.alpha()<64)continue;
                            int diff=qAbs(c.red()-bg.red())+qAbs(c.green()-bg.green())+qAbs(c.blue()-bg.blue())+qAbs(c.alpha()-bg.alpha());
                            if(diff>bestDiff){bestDiff=diff;fg=c;}
                        }
                        if(bestDiff<40)fg=(bg.alpha()>128&&bg.lightness()<90)?QColor(235,235,238,255):QColor(22,22,25,255);
                        else fg.setAlpha(255);
                        for(int y=scan.top();y<=scan.bottom();++y)for(int x=scan.left();x<=scan.right();++x)img->setPixelColor(x,y,bg);
                    }
                    Guard g;p->save();
                    QFont f=p->font();f.setBold(true);f.setPixelSize(qBound(11,r.height()/3,24));
                    p->setFont(f);p->setPen(fg);
                    realRect(p,r.adjusted(0,-1,0,-1),Qt::AlignCenter,label,nullptr);
                    p->restore();
                }
            }
        }
    }
}
static void __fastcall point(QPainter*p,const QPointF&at,const QString&s){auto t=translated(p,s);Guard g;realPoint(p,at,t);}
static void __fastcall pointFlags(QPainter*p,const QPointF&at,const QString&s,int flags,int pad){auto t=translated(p,s);Guard g;realPointFlags(p,at,t,flags,pad);}
static void __fastcall rectF(QPainter*p,const QRectF&r,int flags,const QString&s,QRectF*br){auto t=translated(p,s);Guard g;realRectF(p,r,flags,t,br);}
static void __fastcall rect(QPainter*p,const QRect&r,int flags,const QString&s,QRect*br){auto t=translated(p,s);Guard g;realRect(p,r,flags,t,br);}
static void __fastcall option(QPainter*p,const QRectF&r,const QString&s,const QTextOption&o){auto t=translated(p,s);Guard g;realOption(p,r,t,o);}
static void __fastcall staticText(QPainter*p,const QPointF&at,const QStaticText&s){
    QString t=translated(p,s.text());Guard g;if(t==s.text()){realStatic(p,at,s);return;}
    QStaticText copy(s);copy.setText(t);realStatic(p,at,copy);
}
static void __fastcall item(QPainter*p,const QPointF&at,const QTextItem&s){
    QString t=translated(p,s.text());Guard g;if(t==s.text()){realItem(p,at,s);return;}
    p->save();p->setFont(s.font());p->drawText(at,t);p->restore();
}
static QJsonObject install(Dictionary*d){
    if(installed)return {{"installed",true},{"hooks",addresses.size()}};
    auto info=readJson(rootPath+"/build-info.json");
    QFile library(QCoreApplication::applicationDirPath()+"/Qt6Gui.dll");
    if(!library.open(QIODevice::ReadOnly))return {{"installed",false},{"reason","Qt6Gui unavailable"}};
    QString actual=QString::fromLatin1(QCryptographicHash::hash(library.readAll(),QCryptographicHash::Sha256).toHex());
    if(actual!=info.value("qt_gui_sha256").toString())return {{"installed",false},{"reason","Qt6Gui hash changed"}};
    QFile widgets(QCoreApplication::applicationDirPath()+"/Qt6Widgets.dll");
    if(!widgets.open(QIODevice::ReadOnly)||QString::fromLatin1(QCryptographicHash::hash(widgets.readAll(),QCryptographicHash::Sha256).toHex())!=info.value("qt_widgets_sha256").toString())return {{"installed",false},{"reason","Qt6Widgets hash changed"}};
    QFile core(QCoreApplication::applicationDirPath()+"/DzCore.dll");
    if(!core.open(QIODevice::ReadOnly)||QString::fromLatin1(QCryptographicHash::hash(core.readAll(),QCryptographicHash::Sha256).toHex())!=info.value("application_core_sha256").toString())return {{"installed",false},{"reason","Daz core hash changed"}};
    auto status=MH_Initialize();if(status!=MH_OK&&status!=MH_ERROR_ALREADY_INITIALIZED)return {{"installed",false},{"reason",QString::fromLatin1(MH_StatusToString(status))}};
    struct Entry{const char*symbol;void*adapter;void**original;};
    Entry entries[]={
        {"?drawText@QPainter@@QEAAXAEBVQPointF@@AEBVQString@@@Z",reinterpret_cast<void*>(&point),reinterpret_cast<void**>(&realPoint)},
        {"?drawText@QPainter@@QEAAXAEBVQPointF@@AEBVQString@@HH@Z",reinterpret_cast<void*>(&pointFlags),reinterpret_cast<void**>(&realPointFlags)},
        {"?drawText@QPainter@@QEAAXAEBVQRectF@@HAEBVQString@@PEAV2@@Z",reinterpret_cast<void*>(&rectF),reinterpret_cast<void**>(&realRectF)},
        {"?drawText@QPainter@@QEAAXAEBVQRect@@HAEBVQString@@PEAV2@@Z",reinterpret_cast<void*>(&rect),reinterpret_cast<void**>(&realRect)},
        {"?drawText@QPainter@@QEAAXAEBVQRectF@@AEBVQString@@AEBVQTextOption@@@Z",reinterpret_cast<void*>(&option),reinterpret_cast<void**>(&realOption)},
        {"?drawStaticText@QPainter@@QEAAXAEBVQPointF@@AEBVQStaticText@@@Z",reinterpret_cast<void*>(&staticText),reinterpret_cast<void**>(&realStatic)},
        {"?drawTextItem@QPainter@@QEAAXAEBVQPointF@@AEBVQTextItem@@@Z",reinterpret_cast<void*>(&item),reinterpret_cast<void**>(&realItem)}
    };
    QJsonArray errors;
    for(auto&e:entries){
        void*address=reinterpret_cast<void*>(QLibrary::resolve(library.fileName(),e.symbol));
        if(!address){errors.append(QString::fromLatin1(e.symbol));continue;}
        auto code=MH_CreateHook(address,e.adapter,e.original);
        if(code==MH_OK)addresses.append(address);else errors.append(QString::fromLatin1(e.symbol)+": "+QString::fromLatin1(MH_StatusToString(code)));
    }
    Entry native[]={
        {"?drawControl@DzStyle@@UEBAXW4ControlElement@QStyle@@PEBVQStyleOption@@PEAVQPainter@@PEBVQWidget@@@Z",reinterpret_cast<void*>(&control),reinterpret_cast<void**>(&realControl)},
        {"?drawPrimitive@DzStyle@@UEBAXW4PrimitiveElement@QStyle@@PEBVQStyleOption@@PEAVQPainter@@PEBVQWidget@@@Z",reinterpret_cast<void*>(&primitive),reinterpret_cast<void**>(&realPrimitive)},
        {"?paint@DzCameraCubeCombinedTexture@@MEAAXPEAVQPainter@@@Z",reinterpret_cast<void*>(&cubePaint),reinterpret_cast<void**>(&realCubePaint)}
    };
    for(auto&e:native){void*address=reinterpret_cast<void*>(QLibrary::resolve(core.fileName(),e.symbol));
        if(!address){errors.append(QString::fromLatin1(e.symbol));continue;}
        auto code=MH_CreateHook(address,e.adapter,e.original);if(code==MH_OK)addresses.append(address);else errors.append(QString::fromLatin1(e.symbol)+": "+QString::fromLatin1(MH_StatusToString(code)));
    }
    Entry dialogs[]={
        {"?setWindowTitle@QWidget@@QEAAXAEBVQString@@@Z",reinterpret_cast<void*>(&title),reinterpret_cast<void**>(&realTitle)},
        {"?setText@QMessageBox@@QEAAXAEBVQString@@@Z",reinterpret_cast<void*>(&message),reinterpret_cast<void**>(&realMessage)},
        {"?setInformativeText@QMessageBox@@QEAAXAEBVQString@@@Z",reinterpret_cast<void*>(&information),reinterpret_cast<void**>(&realInformation)}
    };
    for(auto&e:dialogs){void*address=reinterpret_cast<void*>(QLibrary::resolve(widgets.fileName(),e.symbol));
        if(!address){errors.append(QString::fromLatin1(e.symbol));continue;}
        auto code=MH_CreateHook(address,e.adapter,e.original);if(code==MH_OK)addresses.append(address);else errors.append(QString::fromLatin1(e.symbol)+": "+QString::fromLatin1(MH_StatusToString(code)));
    }
    dictionary.store(d,std::memory_order_release);
    auto enabled=MH_EnableHook(MH_ALL_HOOKS);installed=enabled==MH_OK&&!addresses.isEmpty();
    if(!installed)dictionary.store(nullptr,std::memory_order_release);
    return {{"installed",installed},{"hooks",addresses.size()},{"errors",errors},{"scope","GUI-thread UI painting, native style copies, and display-only dialog text; code/input/model/image data are preserved"}};
}
static void stop(){dictionary.store(nullptr,std::memory_order_release);if(installed)MH_DisableHook(MH_ALL_HOOKS);installed=false;}
}
