// SPDX-License-Identifier: MIT
#pragma once
#include <QGenericPlugin>
class DazChinesePlugin final:public QGenericPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QGenericPluginFactoryInterface" FILE "dazzhcn.json")
public:
    QObject*create(const QString&key,const QString&specification)override;
};
