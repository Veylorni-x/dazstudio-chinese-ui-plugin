// SPDX-License-Identifier: MIT
// Display-only Qt extension. No DAZ scene, asset, property, or database setters.
#include <QtWidgets>
#include <QGenericPlugin>
#include <QTranslator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <QMutex>
#include <QMutexLocker>
#include <QElapsedTimer>
#include <atomic>
#include <memory>
#include "plugin.h"
#include <QQuickItem>
#include <QQuickWindow>
#include <QQuickWidget>
#include <windows.h>

static QString rootPath;
static QString dllFilePath() {
    HMODULE hMod = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&dllFilePath), &hMod) || !hMod) return {};
    wchar_t buf[MAX_PATH * 4] = {};
    DWORD len = GetModuleFileNameW(hMod, buf, MAX_PATH * 4);
    return len ? QDir::fromNativeSeparators(QString::fromWCharArray(buf, int(len))) : QString();
}
static QString resolveRootPath() {
    QString env = qEnvironmentVariable("DAZ_ZHCN_ROOT");
    if (!env.isEmpty() && QFileInfo::exists(env + "/dictionary/zh_CN.json")) return QDir::fromNativeSeparators(env);
    QString dll = dllFilePath();
    if (!dll.isEmpty()) {
        QFileInfo fi(dll);
        QFile marker(fi.absolutePath() + "/dazzhcn-root.txt");
        if (marker.open(QIODevice::ReadOnly)) {
            QString candidate = QString::fromUtf8(marker.readAll()).trimmed();
            if (!candidate.isEmpty() && QFileInfo::exists(candidate + "/dictionary/zh_CN.json"))
                return QDir::fromNativeSeparators(candidate);
        }
        QDir d = fi.absoluteDir();
        for (int i = 0; i < 4; ++i) {
            if (QFileInfo::exists(d.absolutePath() + "/dictionary/zh_CN.json")) return QDir::fromNativeSeparators(d.absolutePath());
            if (!d.cdUp()) break;
        }
    }
    QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (!localAppData.isEmpty()) {
        QString candidate = QDir::fromNativeSeparators(localAppData + "/DazStudioZhCN");
        if (QFileInfo::exists(candidate + "/dictionary/zh_CN.json")) return candidate;
    }
    return env;
}
static void saveJson(const QString &name, const QJsonObject &data) {
    QSaveFile f(rootPath + "/reports/" + name);
    if (f.open(QIODevice::WriteOnly)) { f.write(QJsonDocument(data).toJson()); f.commit(); }
}
static QJsonObject readJson(const QString &path) {
    QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

class FrameworkTranslator final:public QTranslator{
    QList<QTranslator*> catalogs;
public:
    void loadFiles(const QString&folder){
        for(const QString&name:{QString("qtbase"),QString("qtdeclarative"),QString("qtwebengine"),QString("qt_help"),QString("qtmultimedia"),QString("qtconnectivity"),QString("qtlocation"),QString("qtserialport"),QString("qtwebsockets")}){
            auto*t=new QTranslator(this);if(t->load(folder+'/'+name+"_zh_CN.qm"))catalogs.append(t);else delete t;
        }
    }
    bool isEmpty()const override{return catalogs.isEmpty();}
    QString translate(const char*c,const char*s,const char*d=nullptr,int n=-1)const override{
        for(auto*t:catalogs){auto result=t->translate(c,s,d,n);if(!result.isNull())return result;}return {};
    }
};

class Dictionary final : public QTranslator {
public:
    std::atomic<bool> enabled{true};
    std::atomic<bool> nativeVisuals{true};
    int revision = 0;
    QHash<QString,QString> pairs, contexts, reverse;
    mutable QHash<QString,QJsonObject> seen;
    mutable QMutex mutex;
    struct Rule{QRegularExpression regex;QString translation,prefix;QStringList markers;};
    QList<Rule> rules;
    mutable QHash<QString,QString> cache;
    QSet<QString> nativeUiContexts,nativeUiSources;
    QHash<QString,QString> nativeUiTranslations;
    QHash<QObject*,QHash<QString,QString>> nativeUiOriginals;
    void rememberUi(QObject*object,const QString&key,const QString&value){
        QMutexLocker lock(&mutex);
        if(!nativeUiOriginals.contains(object))connect(object,&QObject::destroyed,this,[this,object]{QMutexLocker l(&mutex);nativeUiOriginals.remove(object);});
        nativeUiOriginals[object].insert(key,value);
    }
    static QRegularExpression markerRegex(){return QRegularExpression("%(?:[-+ #0]*[0-9]*(?:\\.[0-9]+)?[sdfugxX])|%L?[0-9]+|%n");}
    Dictionary(QObject *p):QTranslator(p){}
    bool isEmpty() const override { return false; }
    bool reload() {
        QHash<QString,QString> next, ctx;
        for (const QString &file : {QString("zh_CN.json"),QString("overrides.json")}) {
            QFile f(rootPath + "/dictionary/" + file);
            if (!f.open(QIODevice::ReadOnly)) return false;
            QJsonParseError error;
            auto obj=QJsonDocument::fromJson(f.readAll(), &error).object();
            if(error.error!=QJsonParseError::NoError || !obj.value("strings").isObject()) return false;
            auto strings=obj.value("strings").toObject();
            for(auto i=strings.begin();i!=strings.end();++i)
                if(i.value().isString()&&!i.value().toString().isEmpty()) next.insert(i.key(),i.value().toString());
            auto c=obj.value("contexts").toObject();
            for(auto i=c.begin();i!=c.end();++i)if(i.value().isString())ctx.insert(i.key(),i.value().toString());
        }
        QList<Rule> nextRules;
        auto markers=markerRegex();
        for(auto i=next.begin();i!=next.end();++i){
            if(!i.key().contains('%'))continue;
            auto matches=markers.globalMatch(i.key());Rule rule;QString expression="^";qsizetype from=0;int literals=0;
            while(matches.hasNext()){
                auto match=matches.next();QString literal=i.key().mid(from,match.capturedStart()-from);
                if(rule.markers.isEmpty())rule.prefix=literal;
                literals+=literal.size();expression+=QRegularExpression::escape(literal)+"(.+?)";
                rule.markers.append(match.captured());from=match.capturedEnd();
            }
            if(rule.markers.isEmpty())continue;
            QString tail=i.key().mid(from);literals+=tail.size();if(literals<6)continue;
            QString meaningful=i.key();meaningful.remove(markers);meaningful.remove(QRegularExpression("<[^>]+>"));
            if(!meaningful.contains(QRegularExpression("[A-Za-z]{4}")))continue;
            if(!i.value().contains(QRegularExpression("[\\x{3400}-\\x{9fff}]")))continue;
            expression+=QRegularExpression::escape(tail)+"$";rule.regex=QRegularExpression(expression,QRegularExpression::DotMatchesEverythingOption);
            if(!rule.regex.isValid())continue;rule.regex.optimize();rule.translation=i.value();nextRules.append(rule);
        }
        auto policy=readJson(rootPath+"/dictionary/display-policy.json");
        QSet<QString> uiContexts,uiSources;
        QHash<QString,QString> uiTranslations;
        for(const auto&v:policy.value("native_ui_contexts").toArray())uiContexts.insert(v.toString());
        for(const auto&v:policy.value("native_ui_sources").toArray())uiSources.insert(v.toString());
        auto specific=policy.value("native_ui_translations").toObject();
        for(auto i=specific.begin();i!=specific.end();++i)if(i.value().isString())uiTranslations.insert(i.key(),i.value().toString());
        QMutexLocker lock(&mutex); pairs.swap(next);contexts.swap(ctx);rules.swap(nextRules);nativeUiContexts.swap(uiContexts);nativeUiSources.swap(uiSources);nativeUiTranslations.swap(uiTranslations);reverse.clear();cache.clear();revision++;
        for(auto i=pairs.begin();i!=pairs.end();++i) if(i.key()!=i.value()&&!reverse.contains(i.value())){
            reverse.insert(i.value(),i.key());QString plain=i.value();plain.remove('&');
            if(plain!=i.value()&&!reverse.contains(plain)){QString source=i.key();source.remove('&');reverse.insert(plain,source);}
        }
        return true;
    }
    static bool protectedText(const QString &s) {
        return s.startsWith("http:")||s.startsWith("https:")||s.startsWith("file:")||s.startsWith("\\\\")||
            (s.size()>2&&s.at(1)==':')||s.startsWith('/')||s.isEmpty()||s.size()>12000;
    }
    QString lookup(const QString &s, const QString &context=QString(), bool audit=true) const {
        if(protectedText(s))return {};
        QMutexLocker lock(&mutex);
        if(reverse.contains(s))return {};
        QString cacheKey=context+QChar(4)+s;
        auto cached=cache.constFind(cacheKey);if(cached!=cache.cend())return cached.value();
        QString out=contexts.value(context+QChar(4)+s);
        if(out.isNull()&&context.startsWith("Paint:")){
            if(context.contains("Combo",Qt::CaseInsensitive))out=contexts.value(QStringLiteral("ComboDisplay\x04")+s);
            else if(context.contains("View",Qt::CaseInsensitive)||context.contains("Tree",Qt::CaseInsensitive)||context.contains("List",Qt::CaseInsensitive))
                out=contexts.value(QStringLiteral("ModelDisplay\x04")+s);
        }
        if(out.isNull())out=pairs.value(s);
        if(out.isNull()) {
            QString n=s; n.remove('&'); n=n.trimmed();
            QString suffix;
            if(n.endsWith("...")){n.chop(3);suffix="…";}
            else if(n.endsWith(QChar(0x2026))){n.chop(1);suffix="…";}
            bool colon=n.endsWith(':'); if(colon)n.chop(1);
            QString trimmed=n.trimmed();
            out=contexts.value(context+QChar(4)+trimmed);
            if(out.isNull()&&context.startsWith("Paint:")){
                if(context.contains("Combo",Qt::CaseInsensitive))out=contexts.value(QStringLiteral("ComboDisplay\x04")+trimmed);
                else if(context.contains("View",Qt::CaseInsensitive)||context.contains("Tree",Qt::CaseInsensitive)||context.contains("List",Qt::CaseInsensitive))
                    out=contexts.value(QStringLiteral("ModelDisplay\x04")+trimmed);
            }
            if(out.isNull())out=pairs.value(trimmed);
            if(!out.isNull()) {
                if(colon&&!out.endsWith(QChar(0xff1a)))out+=QChar(0xff1a);
                out+=suffix;
                int amp=s.indexOf('&');
                if(amp>=0&&amp+1<s.size()&&s.at(amp+1).isLetter()&&!out.contains('&'))out+="(&"+s.mid(amp+1,1).toUpper()+")";
            }
        }
        auto firstChinese=s.indexOf(QRegularExpression("[\\x{3400}-\\x{9fff}]"));
        auto firstEnglish=s.indexOf(QRegularExpression("[A-Za-z]"));
        bool englishMessage=firstEnglish>=0&&firstChinese<0;
        if(out.isNull()&&englishMessage)for(const auto&rule:rules){
            if(!rule.prefix.isEmpty()&&!s.startsWith(rule.prefix))continue;
            auto match=rule.regex.match(s);if(!match.hasMatch())continue;
            QHash<QString,QStringList> values;for(qsizetype i=0;i<rule.markers.size();i++)values[rule.markers.at(i)].append(match.captured(i+1));
            QHash<QString,int> occurrence;auto matches=markerRegex().globalMatch(rule.translation);QString target;qsizetype from=0;
            while(matches.hasNext()){
                auto token=matches.next();target+=rule.translation.mid(from,token.capturedStart()-from);QString name=token.captured();
                auto list=values.value(name);int index=occurrence[name]++;
                if(name.size()>1&&(name.at(1).isDigit()||name.at(1)=='L'||name=="%n"))index=0;
                target+=list.isEmpty()?name:list.at(qMin(index,int(list.size()-1)));from=token.capturedEnd();
            }
            out=target+rule.translation.mid(from);break;
        }
        if(audit && seen.size()<50000 && firstEnglish>=0 && (firstChinese<0 || !out.isNull())) {
            QString key=context+QChar(4)+s;
            seen.insert(key,{{"context",context},{"source",s},{"translation",out},{"translated",!out.isNull()}});
        }
        if(cache.size()>50000)cache.clear();cache.insert(cacheKey,out);
        return out;
    }
    QString display(const QString&s,const QString&context=QString())const {
        if(!enabled)return s;auto out=lookup(s,context);return out.isNull()?s:out;
    }
    QString richText(const QString&s)const{
        if(!enabled)return s;
        auto whole=lookup(s,"RichText");if(!whole.isNull())return whole;
        QList<QPair<qsizetype,qsizetype>> protectedBlocks;
        QRegularExpression codeBlocks(R"rx(<(code|pre|script|style)\b[^>]*>[\s\S]*?</\1>|<(?:span|p)\b[^>]*(?:font-family[^>]*(?:monospace|Consolas|Courier)|class=['"][^'"]*(?:code|source))[^>]*>[\s\S]*?</(?:span|p)>)rx",QRegularExpression::CaseInsensitiveOption);
        auto blocks=codeBlocks.globalMatch(s);while(blocks.hasNext()){auto b=blocks.next();protectedBlocks.append({b.capturedStart(),b.capturedEnd()});}
        QRegularExpression regex("(^|>)([^<>]+)(?=<|$)");
        auto matches=regex.globalMatch(s);QString out;qsizetype from=0;
        while(matches.hasNext()){
            auto m=matches.next();QString raw=m.captured(2),clean=raw.trimmed();
            bool protectedText=false;for(const auto&block:protectedBlocks)if(m.capturedStart(2)>=block.first&&m.capturedStart(2)<block.second){protectedText=true;break;}
            if(protectedText){out+=s.mid(from,m.capturedEnd(2)-from);from=m.capturedEnd(2);continue;}
            auto zh=lookup(clean,"RichTextNode");
            out+=s.mid(from,m.capturedStart(2)-from);
            if(!zh.isNull()){
                qsizetype start=raw.indexOf(clean);out+=raw.left(start)+zh+raw.mid(start+clean.size());
            }else out+=raw;
            from=m.capturedEnd(2);
        }
        return out+s.mid(from);
    }
    QString plainText(const QString&s)const{
        if(!enabled)return s;auto whole=lookup(s,"ReadOnlyText");if(!whole.isNull())return whole;
        QStringList lines=s.split('\n');for(auto&line:lines){auto clean=line.trimmed();auto result=lookup(clean,"ReadOnlyLine");if(!result.isNull())line.replace(clean,result);}return lines.join('\n');
    }
    QString original(const QString&s)const {QMutexLocker lock(&mutex);return reverse.value(s,s);}
    QString translate(const char *ctx,const char *src,const char *,int n)const override {
        if(!src)return {};
        QString context=QString::fromUtf8(ctx?ctx:"");
        QString source=QString::fromUtf8(src);
        bool visualContext=false;QString visualTranslation;
        {QMutexLocker lock(&mutex);visualContext=nativeUiContexts.contains(context)&&nativeUiSources.contains(source);if(visualContext)visualTranslation=nativeUiTranslations.value(source);}
        if(nativeVisuals&&visualContext)return visualTranslation.isNull()?lookup(source,context):visualTranslation;
        if(!enabled)return {};
        // Restrict the native translator to UI classes. Data/schema constructors
        // also call tr(); translating those could persist translated labels.
        // Daz tr() is also used for default object labels and enum values. Keep
        // those raw. Qt standard dialogs use the official Qt language catalog;
        // Daz captions are localized only after reaching a display widget.
        if(!context.startsWith('Q')||context=="QObject"||context=="QGuiApplication")return {};
        auto s=lookup(QString::fromUtf8(src),context);
        if(!s.isNull()&&n>=0)s.replace("%n",QString::number(n));return s;
    }
    QJsonObject report() const {
        QMutexLocker lock(&mutex); QJsonArray list;int translated=0;
        for(const auto &v:seen){list.append(v);if(v.value("translated").toBool())translated++;}
        return {{"enabled",enabled.load()},{"dictionaryEntries",pairs.size()},{"seen",seen.size()},{"matched",translated},{"entries",list}};
    }
};

#include "paintadapter.h"
#include "graphicstext.h"

extern "C" __declspec(dllexport) void* getPluginDefinition();

class Controller final: public QObject {
    Dictionary *dict;
    FrameworkTranslator qtBase;
    QTimer timer;
    bool sweeping=false;
    struct TextState{QString source,last;int revision=-1;};
    QHash<QObject*,QHash<QByteArray,TextState>> originals;
    QString lastCommand;
    int sweeps=0;
    QPointer<QMenu> languageMenu;
    QPointer<QToolBar> languageBar;
    QList<QPointer<QAction>>headerActions;
    QElapsedTimer age;
    qint64 windowSeenAt=-1;
    bool ready=false, requestedChinese=true;
    bool closing=false, movedToSecondary=false, nativeRegistered=false, nativeInPluginMgr=false;
    QJsonObject helpFiles;
public:
    void registerNativePlugin(){
        if(nativeRegistered)return;nativeRegistered=true;
        HMODULE core=GetModuleHandleW(L"DzCore.dll");if(!core)return;
        auto*dzAppPtr=reinterpret_cast<void**>(GetProcAddress(core,"?dzApp@@3PEAVDzApp@@EA"));
        if(!dzAppPtr||!*dzAppPtr)return;
        using GetMgrFn=void*(__fastcall*)(void*);
        using FindFn=void*(__fastcall*)(void*,const QString&);
        using SetFileFn=void(__fastcall*)(void*,const QString&);
        using SetLoadFn=void(__fastcall*)(void*,bool);
        using SetStatusFn=void(__fastcall*)(void*,int,const QString&);
        using AddPluginFn=void(__fastcall*)(void*,void*);
        auto getMgr=reinterpret_cast<GetMgrFn>(GetProcAddress(core,"?getPluginMgr@DzApp@@QEBAPEAVDzPluginMgr@@XZ"));
        auto findPlugin=reinterpret_cast<FindFn>(GetProcAddress(core,"?findPlugin@DzPluginMgr@@QEBAPEAVDzPlugin@@AEBVQString@@@Z"));
        auto setFileName=reinterpret_cast<SetFileFn>(GetProcAddress(core,"?setFileName@DzPlugin@@IEAAXAEBVQString@@@Z"));
        auto setLoadOnStart=reinterpret_cast<SetLoadFn>(GetProcAddress(core,"?setLoadOnStart@DzPlugin@@IEAAX_N@Z"));
        auto setStatus=reinterpret_cast<SetStatusFn>(GetProcAddress(core,"?setStatus@DzPlugin@@IEAAXW4Status@1@AEBVQString@@@Z"));
        auto addPlugin=reinterpret_cast<AddPluginFn>(GetProcAddress(core,"?addPlugin@DzPluginMgr@@AEAAXPEAVDzPlugin@@@Z"));
        if(!getMgr||!findPlugin||!addPlugin)return;
        void*mgr=getMgr(*dzAppPtr);if(!mgr)return;
        QString pluginName=QString::fromUtf8("Daz Studio 简体中文 (Simplified Chinese)");
        if(!findPlugin(mgr,pluginName)){
            void*plugin=getPluginDefinition();
            if(!plugin)return;
            if(setFileName)setFileName(plugin,dllFilePath());
            if(setLoadOnStart)setLoadOnStart(plugin,true);
            if(setStatus)setStatus(plugin,0,QString::fromUtf8("已加载 / Loaded"));
            addPlugin(mgr,plugin);
        }
        nativeInPluginMgr=findPlugin(mgr,pluginName)!=nullptr;
        saveJson("plugin-status.json",{{"loaded",true},{"qt",qVersion()},{"pluginVersion","1.0.0"},{"nativeDzPlugin",nativeInPluginMgr},{"sdkVersion","6.26.2026.30200"},{"root",rootPath}});
    }
    void refreshViewports(){
        HMODULE core=GetModuleHandleW(L"DzCore.dll");if(!core)return;
        using GetCubeFn=QObject*(__fastcall*)(QWidget*);
        using InvalidateFn=void(__fastcall*)(QObject*);
        static auto getCube=reinterpret_cast<GetCubeFn>(GetProcAddress(core,"?getCameraCube@Dz3DViewport@@QEBAPEAVDzCameraCube@@XZ"));
        static auto invalidate=reinterpret_cast<InvalidateFn>(GetProcAddress(core,"?textureInvalidated@DzCameraCube@@QEAAXXZ"));
        if(!getCube||!invalidate)return;
        for(auto*w:QApplication::allWidgets()){
            if(QString::fromLatin1(w->metaObject()->className())=="Dz3DViewport"){
                if(auto*cube=getCube(w))invalidate(cube);
                w->update();
            }
        }
    }
    void ensureSecondaryScreen(){
        if(movedToSecondary||qEnvironmentVariable("DAZ_ZHCN_TEST").isEmpty())return;
        auto*window=mainWindow();if(!window)return;
        for(auto*screen:QGuiApplication::screens())if(screen!=QGuiApplication::primaryScreen()){
            bool maximized=window->isMaximized();
            if(maximized)window->showNormal();
            if(window->windowHandle())window->windowHandle()->setScreen(screen);
            window->move(screen->availableGeometry().topLeft()+QPoint(20,20));
            if(maximized)window->showMaximized();
            movedToSecondary=true;break;
        }
    }
    Controller(QObject*p):QObject(p),dict(new Dictionary(this)) {
        QDir().mkpath(rootPath+"/reports/screenshots");
        dict->reload();
        requestedChinese=readJson(rootPath+"/settings.json").value("chinese").toBool(true);
        dict->enabled=requestedChinese;dict->nativeVisuals=requestedChinese;age.start();
        helpFiles=readJson(rootPath+"/help/local-help-map.json");
        qtBase.loadFiles(rootPath+"/runtime");
        if(readJson(rootPath+"/settings.json").value("paint_adapter").toBool(true))saveJson("paint-adapter.json",PaintAdapter::install(dict));
        QCoreApplication::installTranslator(dict);
        if(dict->enabled)QCoreApplication::installTranslator(&qtBase);
        QTimer::singleShot(0,this,[this]{
            connect(&timer,&QTimer::timeout,this,[this]{
                ensureSecondaryScreen();
                if(!ready){
                    if(auto*modal=QApplication::activeModalWidget()){
                        widget(modal);for(auto*child:modal->findChildren<QWidget*>())widget(child);
                    }
                    if(mainWindow()&&mainWindow()->isVisible()&&!mainWindow()->windowTitle().isEmpty()){
                        if(windowSeenAt<0)windowSeenAt=age.elapsed();
                        if(age.elapsed()-windowSeenAt>=500){
                            ready=true;
                            ensureSecondaryScreen();
                            registerNativePlugin();
                            qApp->installEventFilter(this);
                            if(qEnvironmentVariable("DAZ_ZHCN_TRANSLATOR_ONLY").isEmpty())sweep();
                            refreshViewports();
                            for(auto*w:QApplication::allWidgets())w->update();
                        }
                    }
                }
                if(ready&&qEnvironmentVariable("DAZ_ZHCN_TRANSLATOR_ONLY").isEmpty())sweep();
                if(ready)command();
            });timer.start(750);
            connect(qApp,&QCoreApplication::aboutToQuit,this,[this]{
                prepareForClose();PaintAdapter::stop();
                saveJson("coverage-runtime.json",dict->report());
            });
            saveJson("plugin-status.json",{{"loaded",true},{"qt",qVersion()},{"pluginVersion","0.2.0"},{"root",rootPath}});
        });
    }
    ~Controller(){timer.stop();PaintAdapter::stop();}
    void prepareForClose(){
        closing=true;timer.stop();
        for(auto&action:headerActions)if(action){auto state=originals.value(action).value("text");if(!state.source.isNull())action->setText(state.source);}
        if(languageBar){if(auto*m=mainWindow())m->removeToolBar(languageBar);languageBar->hide();}
    }
    template<typename Getter,typename Setter>void text(QObject*o,const QByteArray&key,Getter get,Setter set){
        QString current=get();if(current.isEmpty()||Dictionary::protectedText(current))return;
        if(!originals.contains(o))connect(o,&QObject::destroyed,this,[this,o]{originals.remove(o);});
        auto state=originals[o].value(key);
        if(current==state.last&&state.revision==dict->revision)return;
        bool equivalent=false;
        if(!state.last.isNull()&&current!=state.last){
            auto plain=[](const QString&s){QString p=Qt::mightBeRichText(s)?QTextDocumentFragment::fromHtml(s).toPlainText():s;p.remove('&');return p.simplified();};
            equivalent=plain(current)==plain(state.last);
        }
        if(state.last.isNull()||(current!=state.last&&!equivalent))state.source=dict->original(current);
        QString target=key=="html"?dict->richText(state.source):(key=="plain"?dict->plainText(state.source):dict->display(state.source,o->metaObject()->className()));
        state.last=target;state.revision=dict->revision;originals[o][key]=state;
        QPointer<QObject> alive=o;
        if(target!=current){set(target);if(alive)originals[o][key].last=get();}
    }
    void property(QObject*o,const char*name){
        text(o,name,[o,name]{return o->property(name).toString();},[o,name](const QString&s){o->setProperty(name,s);});
    }
    void widget(QWidget*w){
        if(w->property("dzcn_own").toBool())return;
        for(auto p:{"toolTip","statusTip","whatsThis"})property(w,p);
        if(auto*label=qobject_cast<QLabel*>(w)){
            bool rich=label->textFormat()==Qt::RichText||(label->textFormat()==Qt::AutoText&&Qt::mightBeRichText(label->text()));
            text(label,rich?"html":"text",[label]{return label->text();},[label](const QString&s){label->setText(s);});
        }
        if(qobject_cast<QGroupBox*>(w))property(w,"title");
        if(qobject_cast<QLineEdit*>(w))property(w,"placeholderText");
        bool mappedHelp=false;
        for(auto propertyName:{"source","url"}){
            if(w->metaObject()->indexOfProperty(propertyName)<0)continue;
            auto value=w->property(propertyName);if(!value.canConvert<QUrl>())continue;
            auto url=value.toUrl();if(!url.isLocalFile())continue;
            QString path=QDir::cleanPath(url.toLocalFile()).toLower();
            auto local=helpFiles.value(path).toString();
            if(!local.isEmpty()){
                mappedHelp=true;
                if(dict->enabled){auto translated=QUrl::fromLocalFile(rootPath+"/help/"+local);translated.setFragment(url.fragment());w->setProperty(propertyName,translated);}
            }else if(path.startsWith(QDir::cleanPath(rootPath+"/help/").toLower())){
                for(auto i=helpFiles.begin();i!=helpFiles.end();++i)
                    if(path==QDir::cleanPath(rootPath+"/help/"+i.value().toString()).toLower()){
                        mappedHelp=true;if(!dict->enabled){auto original=QUrl::fromLocalFile(i.key());original.setFragment(url.fragment());w->setProperty(propertyName,original);}break;
                    }
            }
        }
        if(!mappedHelp)if(auto*browser=qobject_cast<QTextBrowser*>(w))
            text(browser,"html",[browser]{return browser->toHtml();},[browser](const QString&s){auto base=browser->document()->baseUrl();browser->setHtml(s);browser->document()->setBaseUrl(base);});
        QString identity=QString::fromLatin1(w->metaObject()->className())+' '+w->objectName();
        bool contentEditor=identity.contains(QRegularExpression("script|code|console|log",QRegularExpression::CaseInsensitiveOption));
        if(!mappedHelp&&!contentEditor&&!qobject_cast<QTextBrowser*>(w)){
            if(auto*edit=qobject_cast<QTextEdit*>(w))if(edit->isReadOnly())text(edit,"html",[edit]{return edit->toHtml();},[edit](const QString&s){edit->setHtml(s);});
            if(auto*edit=qobject_cast<QPlainTextEdit*>(w))if(edit->isReadOnly())text(edit,"plain",[edit]{return edit->toPlainText();},[edit](const QString&s){edit->setPlainText(s);});
        }
        if(auto*t=qobject_cast<QTabBar*>(w))for(int i=0;i<t->count();i++)
            text(t,"tab"+QByteArray::number(i),[t,i]{return t->tabText(i);},[t,i](const QString&s){t->setTabText(i,s);});
    }
    QMainWindow*mainWindow(){
        for(auto*w:QApplication::topLevelWidgets())if(auto*m=qobject_cast<QMainWindow*>(w))
            if(m->isVisible()&&(m->objectName()=="MainWindow"||QString::fromLatin1(m->metaObject()->className())=="DzMainWindow"))return m;
        for(auto*w:QApplication::topLevelWidgets())if(auto*m=qobject_cast<QMainWindow*>(w))if(m->isVisible())return m;
        return nullptr;
    }
    void menu(){
        auto*m=mainWindow();if(!m||languageMenu)return;
        // DzMainWindow has a custom menu widget. QMainWindow::menuBar() would
        // replace it; use a separate native toolbar without touching that widget.
        languageBar=new QToolBar(QString::fromUtf8("语言 / Language"),m);
        languageBar->setObjectName("DazZhCNLanguageToolbar");languageBar->setProperty("dzcn_own",true);
        m->addToolBar(Qt::TopToolBarArea,languageBar);
        auto*button=new QToolButton(languageBar);button->setText(QString::fromUtf8("语言 / Language"));button->setProperty("dzcn_own",true);
        languageMenu=new QMenu(button);languageMenu->setProperty("dzcn_own",true);
        button->setMenu(languageMenu);button->setPopupMode(QToolButton::InstantPopup);languageBar->addWidget(button);
        auto add=[this](const QString&label,auto fn){auto*a=languageMenu->addAction(label);a->setProperty("dzcn_own",true);connect(a,&QAction::triggered,this,fn);};
        add(QString::fromUtf8("简体中文"),[this]{setChinese(true);});
        add("English",[this]{setChinese(false);});
        languageMenu->addSeparator();
        add(QString::fromUtf8("重新加载词库 / Reload Dictionary"),[this]{bool ok=dict->reload();sweep();for(auto*w:QApplication::allWidgets())w->update();QMessageBox::information(mainWindow(),QString::fromUtf8("词库 / Dictionary"),ok?QString::fromUtf8("词库已重新加载。\nDictionary reloaded."):QString::fromUtf8("词库格式有误，已保留上次的词库。\nInvalid JSON; previous dictionary retained."));});
        add(QString::fromUtf8("导出漏翻清单 / Export Report"),[this]{saveJson("coverage-runtime.json",dict->report());});
        add(QString::fromUtf8("离线中文帮助 / Offline Help"),[this]{
            auto*dialog=new QDialog(mainWindow());dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowTitle(QString::fromUtf8("Daz Studio 离线中文帮助"));dialog->resize(850,680);
            auto*layout=new QVBoxLayout(dialog);auto*browser=new QTextBrowser(dialog);browser->setSource(QUrl::fromLocalFile(rootPath+"/help/index.html"));browser->setOpenExternalLinks(false);layout->addWidget(browser);dialog->show();
        });
    }
    void quickText(QQuickItem*item,QSet<QQuickItem*>&visited){
        if(!item||visited.contains(item))return;visited.insert(item);
        QList<QPointer<QQuickItem>>children;for(auto*child:item->childItems())children.append(child);
        QString type=QString::fromLatin1(item->metaObject()->className());
        if((item->inherits("QQuickText")||type.startsWith("QQuickText"))&&!type.contains("Input")&&!type.contains("Edit")){
            if(item->metaObject()->indexOfProperty("text")>=0)text(item,"text",[item]{return item->property("text").toString();},[item](const QString&s){item->setProperty("text",s);});
        }
        for(auto&child:children)if(child)quickText(child,visited);
    }
    void sweep(){
        if(sweeping||closing)return;sweeping=true;menu();
        // Daz also paints offscreen widget instances into its 3D HUD textures.
        for(auto*w:QApplication::allWidgets())widget(w);
        for(auto*w:QApplication::allWidgets())if(auto*bar=qobject_cast<QMenuBar*>(w)){
            QList<QPointer<QAction>>actions;for(auto*action:bar->actions())actions.append(action);
            for(auto&action:actions)if(action){if(!headerActions.contains(action))headerActions.append(action);
                auto*target=action.data();text(target,"text",[target]{return target->text();},[target](const QString&s){target->setText(s);});}
        }
        QSet<QQuickItem*>quickVisited;
        for(auto*window:QGuiApplication::allWindows()){
            if(auto*quick=qobject_cast<QQuickWindow*>(window))quickText(quick->contentItem(),quickVisited);
            for(auto*item:window->findChildren<QQuickItem*>())quickText(item,quickVisited);
        }
        for(auto*w:QApplication::allWidgets()){
            if(auto*quick=qobject_cast<QQuickWidget*>(w))if(auto*window=quick->quickWindow())quickText(window->contentItem(),quickVisited);
        }
        QSet<QGraphicsScene*>scenes;
        for(auto*w:QApplication::allWidgets())if(auto*view=qobject_cast<QGraphicsView*>(w))if(view->scene()){
            QString id=QString::fromLatin1(w->metaObject()->className())+' '+w->objectName();
            if(!id.contains(QRegularExpression("Script.*(?:Edit|View)|CodeEdit|CodeView|Console|Log(?:View|Edit|Text|Browser)|SourceEditor",QRegularExpression::CaseInsensitiveOption)))scenes.insert(view->scene());
        }
        for(auto*scene:scenes)for(auto*item:scene->items()){
            if(item->graphicsEffect())continue;
            if(auto*textItem=dynamic_cast<QGraphicsTextItem*>(item)){
                if(!(textItem->textInteractionFlags()&Qt::TextEditable)&&dict->display(textItem->toPlainText(),"GraphicsText")!=textItem->toPlainText())item->setGraphicsEffect(new GraphicsTextEffect(dict,textItem));
            }else if(auto*textItem=dynamic_cast<QGraphicsSimpleTextItem*>(item)){
                if(dict->display(textItem->text(),"GraphicsText")!=textItem->text())item->setGraphicsEffect(new GraphicsTextEffect(dict,textItem));
            }
        }
        sweeping=false;sweeps++;
    }
    QList<QAction*>allActions(){
        QSet<QAction*>set;
        for(auto*a:qApp->findChildren<QAction*>())set.insert(a);
        for(auto*w:QApplication::topLevelWidgets())for(auto*a:w->findChildren<QAction*>())set.insert(a);
        for(auto*w:QApplication::allWidgets()){
            for(auto*a:w->actions())set.insert(a);
            if(auto*b=qobject_cast<QToolButton*>(w))if(b->defaultAction())set.insert(b->defaultAction());
        }
        return set.values();
    }
    void setChinese(bool on){
        dict->enabled=on;dict->nativeVisuals=on;dict->revision++;
        PaintAdapter::refreshUi(dict);
        if(on)QCoreApplication::installTranslator(&qtBase);else QCoreApplication::removeTranslator(&qtBase);
        for(auto*w:QApplication::topLevelWidgets()){QEvent event(QEvent::LanguageChange);QCoreApplication::sendEvent(w,&event);}
        QSet<QObject*>uiObjects;
        for(auto*w:QApplication::allWidgets()){
            if(dict->nativeUiContexts.contains(QString::fromLatin1(w->metaObject()->className())))uiObjects.insert(w);
        }
        for(auto*object:uiObjects){QEvent event(QEvent::LanguageChange);QCoreApplication::sendEvent(object,&event);
            if(object->metaObject()->indexOfMethod("rebuildMenu()")>=0)QMetaObject::invokeMethod(object,"rebuildMenu",Qt::DirectConnection);
        }
        auto settings=readJson(rootPath+"/settings.json");settings.insert("chinese",on);
        QSaveFile f(rootPath+"/settings.json");if(f.open(QIODevice::WriteOnly)){f.write(QJsonDocument(settings).toJson());f.commit();}
        sweep();refreshViewports();for(auto*w:QApplication::allWidgets())w->update();
    }
    QJsonObject dump(){
        QJsonArray widgets,actions,models;QSet<QAction*>done;
        for(auto*w:QApplication::allWidgets()){
            QJsonObject v{{"class",w->metaObject()->className()},{"name",w->objectName()},{"visible",w->isVisible()},{"title",w->windowTitle()}};
            for(auto p:{"text","title","placeholderText","toolTip"})if(w->metaObject()->indexOfProperty(p)>=0){auto s=w->property(p).toString();if(!s.isEmpty())v[p]=s;}
            if(auto*cb=qobject_cast<QComboBox*>(w)){v["rawCurrentText"]=cb->currentText();v["paintedCurrentText"]=dict->display(cb->currentText(),"ComboDisplay");}
            if(v.size()>4||w->isWindow())widgets.append(v);
            for(auto*a:w->actions())if(!done.contains(a)){
                done.insert(a);QString raw=originals.value(a).value("text").source;if(raw.isNull())raw=dict->original(a->text());
                actions.append(QJsonObject{{"class",a->metaObject()->className()},{"name",a->objectName()},{"source",raw},{"text",a->text()},{"enabled",a->isEnabled()}});
            }
            if(auto*view=qobject_cast<QAbstractItemView*>(w))if(view->model()){
                QJsonArray rows;auto*model=view->model();auto parent=view->rootIndex();
                for(int row=0;row<qMin(model->rowCount(parent),30);row++){
                    auto index=model->index(row,0,parent);QString raw=index.data().toString();
                    rows.append(QJsonObject{{"raw",raw},{"display",dict->display(raw,"ModelDisplay")}});
                }
                models.append(QJsonObject{{"class",w->metaObject()->className()},{"name",w->objectName()},{"model",model->metaObject()->className()},{"rows",rows}});
            }
        }
        auto out=QJsonObject{{"language",dict->enabled?"zh_CN":"en"},{"widgets",widgets},{"actions",actions},{"models",models}};
        QJsonArray all,meta;
        for(auto*a:allActions()){
            QString raw=originals.value(a).value("text").source;if(raw.isNull())raw=dict->original(a->text());
            all.append(QJsonObject{{"name",a->objectName()},{"class",a->metaObject()->className()},{"source",raw},{"text",a->text()},{"enabled",a->isEnabled()}});
        }
        for(int i=0;i<qApp->metaObject()->methodCount();i++)meta.append(QString::fromLatin1(qApp->metaObject()->method(i).methodSignature()));
        out["actions"]=all;out["appMethods"]=meta;
        saveJson("ui-dump.json",out);saveJson("coverage-runtime.json",dict->report());return out;
    }
    void command(){
        if(qEnvironmentVariable("DAZ_ZHCN_TEST").isEmpty())return;
        auto c=readJson(rootPath+"/reports/command.json");QString id=c.value("id").toString();if(id.isEmpty()||id==lastCommand)return;lastCommand=id;
        QString op=c.value("op").toString();QJsonObject result{{"id",id},{"ok",true}};
        if(op=="dump")result["data"]=dump();
        else if(op=="language")setChinese(c.value("chinese").toBool());
        else if(op=="reload")result["ok"]=dict->reload();
        else if(op=="grab"){
            QString name=c.value("file").toString("main.png");if(name.contains('/')||name.contains('\\')){result["ok"]=false;}
            else{
                QWidget*w=nullptr;QString wanted=c.value("class").toString();
                if(!wanted.isEmpty())for(auto*item:QApplication::allWidgets())if(item->isVisible()&&QString::fromLatin1(item->metaObject()->className())==wanted){w=item;break;}
                if(!w)w=QApplication::activeModalWidget();if(!w)w=QApplication::activePopupWidget();if(!w)w=mainWindow();
                auto pixmap=w?(!wanted.isEmpty()?w->grab():w->screen()->grabWindow(w->winId())):QPixmap();
                if(pixmap.isNull()&&w)pixmap=w->grab();
                result["ok"]=!pixmap.isNull()&&pixmap.save(rootPath+"/reports/screenshots/"+name);
                if(w){result["class"]=w->metaObject()->className();result["title"]=w->windowTitle();}
            }
        }else if(op=="trigger"){
            QString source=c.value("source").toString();QSet<QAction*>set;QAction*target=nullptr;int count=0;
            for(auto*a:allActions())if(!set.contains(a)){
                set.insert(a);QString raw=originals.value(a).value("text").source;if(raw.isNull())raw=dict->original(a->text());
                if(raw==source||a->objectName()==source||QString::fromLatin1(a->metaObject()->className())==source){target=a;count++;}
            }
            result["matches"]=count;result["ok"]=count==1;
            if(count==1){saveJson("response.json",result);QTimer::singleShot(0,target,[target]{target->trigger();});return;}
        }else if(op=="message"){
            auto*box=new QMessageBox(QMessageBox::Warning,dict->display("Warning"),dict->display(c.value("text").toString("Could not open the file.")),QMessageBox::Ok,mainWindow());box->setObjectName("DazZhCNWarningMessage");box->setAttribute(Qt::WA_DeleteOnClose);box->show();
        }else if(op=="closeDialog"){
            QWidget*w=QApplication::activeModalWidget();if(!w)w=QApplication::activeWindow();
            if(auto*box=qobject_cast<QMessageBox*>(w))box->close();else if(auto*dialog=qobject_cast<QDialog*>(w))dialog->reject();else result["ok"]=false;
        }else if(op=="dismissPopup"){
            auto*popup=QApplication::activePopupWidget();result["ok"]=popup!=nullptr;
            if(popup){saveJson("response.json",result);QTimer::singleShot(0,popup,[popup]{popup->close();});return;}
        }else if(op=="popupPaneMenu"){
            QWidget*pane=nullptr;QString wanted=c.value("class").toString();
            for(auto*w:QApplication::allWidgets())if(QString::fromLatin1(w->metaObject()->className())==wanted){pane=w;break;}
            QMenu*menu=pane?new QMenu(pane):nullptr;
            std::function<void(QMenu*,const QJsonArray&,int)>populate=[&](QMenu*target,const QJsonArray&entries,int depth){
                if(!target||depth>5)return;
                for(const auto&value:entries){auto item=value.toObject();int kind=item.value("type").toInt();
                    if(kind==0){target->addSeparator();continue;}
                    QString label=item.value("label").toString().trimmed();
                    if(label.isEmpty())continue;
                    auto*action=target->addAction(label);
                    if(item.value("checkable").toBool())action->setCheckable(true);
                    if(item.value("checked").toBool())action->setChecked(true);
                    const auto children=item.value("children").toArray();
                    if(!children.isEmpty()){auto*submenu=action->menu()?action->menu():new QMenu(action->text(),target);action->setMenu(submenu);populate(submenu,children,depth+1);}
                }
            };
            const auto entries=c.value("items").toArray();populate(menu,entries,0);
            result["ok"]=menu&&!entries.isEmpty();result["sourceItems"]=entries.size();
            if(menu&&!entries.isEmpty()){
                QString file=c.value("file").toString("pane-menu.png");menu->setAttribute(Qt::WA_DeleteOnClose);
                menu->popup(pane->mapToGlobal(QPoint(16,28)));
                QPointer<QMenu>popup=menu;
                QTimer::singleShot(700,this,[this,popup,file,entries]{
                    QPixmap image=popup?popup->grab():QPixmap();bool saved=!image.isNull()&&image.save(rootPath+"/reports/screenshots/"+file);
                    saveJson("popup-screenshot.json",{{"saved",saved},{"file",file},{"class",popup?popup->metaObject()->className():""},{"sourceItems",entries.size()},{"renderer","Daz source menu model displayed in a native Qt popup"}});
                    if(popup)popup->close();
                });
            }else if(menu)menu->deleteLater();
        }else if(op=="inspectUiMethods"){
            QString type=c.value("class").toString();QJsonArray found;int limit=0;
            for(auto*w:QApplication::allWidgets())if(QString::fromLatin1(w->metaObject()->className()).contains(type)){
                QJsonArray methods,properties;auto*m=w->metaObject();
                for(int i=0;i<m->methodCount();i++)methods.append(QString::fromLatin1(m->method(i).methodSignature()));
                for(int i=0;i<m->propertyCount();i++){auto p=m->property(i);properties.append(QJsonObject{{"name",p.name()},{"type",p.typeName()},{"writable",p.isWritable()}});}
                found.append(QJsonObject{{"class",m->className()},{"name",w->objectName()},{"methods",methods},{"properties",properties}});if(++limit>=12)break;
            }
            result["objects"]=found;
        }else if(op=="startupOffline"){
            auto*dialog=QApplication::activeModalWidget();bool found=false;
            for(auto*widget:QApplication::allWidgets()){
                auto*button=qobject_cast<QAbstractButton*>(widget);if(!button)continue;
                QString source=originals.value(button).value("text").source;
                if(source.contains("Offline",Qt::CaseInsensitive)||button->objectName().contains("Offline",Qt::CaseInsensitive)||button->text().contains(QString::fromUtf8("离线"))){
                    if(button->isCheckable())button->setChecked(true);else button->click();found=true;break;
                }
            }
            result["ok"]=found;
            if(found&&dialog){saveJson("response.json",result);QPointer<QWidget>target=dialog;QTimer::singleShot(0,this,[target]{if(target)if(auto*d=qobject_cast<QDialog*>(target.data()))d->accept();});return;}
        }else if(op=="quit")QTimer::singleShot(0,this,[this]{prepareForClose();QCoreApplication::quit();});
        else result["ok"]=false;
        saveJson("response.json",result);
    }
    bool eventFilter(QObject*o,QEvent*e)override{
        if(e->type()==QEvent::Close&&(o->objectName()=="MainWindow"||QString::fromLatin1(o->metaObject()->className())=="DzMainWindow")){
            prepareForClose();
            QTimer::singleShot(0,this,[this]{
                if(closing&&mainWindow()&&mainWindow()->isVisible()){
                    closing=false;
                    if(languageBar){mainWindow()->addToolBar(Qt::TopToolBarArea,languageBar);languageBar->show();}
                    sweep();for(auto*w:QApplication::allWidgets())w->update();
                    timer.start(750);
                }
            });
            return false;
        }
        if(sweeping||!ready||!qEnvironmentVariable("DAZ_ZHCN_TRANSLATOR_ONLY").isEmpty())return false;
        if(e->type()==QEvent::Show||e->type()==QEvent::ToolTip){
            if(auto*w=qobject_cast<QWidget*>(o)){sweeping=true;widget(w);sweeping=false;}
        }
        return false;
    }
};

static QPointer<Controller> activeController;

static Controller* ensureController(QObject* parentForNew) {
    if (activeController) return activeController.data();
    if (QFileInfo(QCoreApplication::applicationFilePath()).fileName().compare("DAZStudio.exe", Qt::CaseInsensitive) != 0) return nullptr;
    QString prefix = QString::number(QT_VERSION_MAJOR) + '.' + QString::number(QT_VERSION_MINOR) + '.';
    if (!QString::fromLatin1(qVersion()).startsWith(prefix)) return nullptr;
    if (rootPath.isEmpty()) rootPath = resolveRootPath();
    if (rootPath.isEmpty()) return nullptr;
    activeController = new Controller(parentForNew);
    return activeController.data();
}

QObject* DazChinesePlugin::create(const QString& key, const QString&) {
    if (key.compare("dazzhcn", Qt::CaseInsensitive) != 0) return nullptr;
    // If already created by Daz native SDK plugin loader, return a lightweight dummy QObject
    // for Qt's generic_plugin_list so Controller is never double-owned.
    if (activeController) return new QObject(nullptr);
    return ensureController(nullptr);
}

// ============================================================================
// Daz Studio 6.26 Native SDK Plugin Interface (DzPlugin ABI)
// Allows dazzhcn.dll to be placed directly in Daz Studio's plugins/ folder
// and loaded natively by DzPluginMgr, appearing in "About Installed Plugins".
// ============================================================================
#pragma pack(push, 1)
struct DzPluginSDKTagData {
    char magic[16];
    uint32_t tagVersion;
    uint16_t major;
    uint16_t minor;
    uint16_t revision;
    uint16_t build;
};
#pragma pack(pop)

#pragma section(".dz_sdk", read)
extern "C" __declspec(allocate(".dz_sdk")) __declspec(dllexport) const DzPluginSDKTagData dzPluginSDKVersionTag = {
    "DzPluginSDKTag", 1, 6, 26, 2026, 30200
};

extern "C" __declspec(dllexport) void* getSDKVersion(void* out) {
    using Fn = void*(__fastcall*)(void*, uint16_t, uint16_t, uint16_t, uint16_t);
    HMODULE core = GetModuleHandleW(L"DzCore.dll");
    static auto ctor = core ? reinterpret_cast<Fn>(GetProcAddress(core, "??0DzVersion@@QEAA@GGGG@Z")) : nullptr;
    if (ctor) return ctor(out, 6, 26, 2026, 30200);
    auto* v = reinterpret_cast<uint16_t*>(out);
    v[0] = 6; v[1] = 26; v[2] = 2026; v[3] = 30200;
    return out;
}

static alignas(16) unsigned char s_dzPluginStorage[64] = {};
static bool s_dzPluginConstructed = false;
static void(__fastcall* s_dzPluginDtor)(void*) = nullptr;

extern "C" __declspec(dllexport) void* getPluginDefinition() {
    HMODULE core = GetModuleHandleW(L"DzCore.dll");
    if (!core) return nullptr;
    using CtorFn = void*(__fastcall*)(void*, const QString&, const QString&, const QString&, uint16_t, uint16_t, uint16_t, uint16_t);
    using DtorFn = void(__fastcall*)(void*);
    using FinishFn = void(__fastcall*)(void*);
    if (!s_dzPluginConstructed) {
        auto ctor = reinterpret_cast<CtorFn>(GetProcAddress(core, "??0DzPlugin@@QEAA@AEBVQString@@00GGGG@Z"));
        s_dzPluginDtor = reinterpret_cast<DtorFn>(GetProcAddress(core, "??1DzPlugin@@UEAA@XZ"));
        if (!ctor) return nullptr;
        QString name = QString::fromUtf8("Daz Studio 简体中文 (Simplified Chinese)");
        QString author = QString::fromUtf8("DazStudioZhCN");
        QString desc = QString::fromUtf8("Daz Studio 6.26 简体中文界面语言扩展插件（支持词库热重载与中英实时切换）。");
        ctor(s_dzPluginStorage, name, author, desc, 1, 0, 0, 0);
        s_dzPluginConstructed = true;
        atexit([] {
            if (s_dzPluginConstructed && s_dzPluginDtor) {
                s_dzPluginDtor(s_dzPluginStorage);
                s_dzPluginConstructed = false;
            }
        });
    }
    auto finish = reinterpret_cast<FinishFn>(GetProcAddress(core, "?finishSetup@DzPlugin@@QEAAXXZ"));
    if (finish) finish(s_dzPluginStorage);
    if (!activeController && qApp) ensureController(qApp);
    return s_dzPluginStorage;
}
