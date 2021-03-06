#ifndef DETAILSVIEWER_H
#define DETAILSVIEWER_H

#include <QDialog>
#include <QObject>
#include <QtGui/QVBoxLayout>
#include <QtGui/QDialogButtonBox>
#include <QtCore/QSignalMapper>
#include <QtGui/QTabWidget>
#include <QtGui/QTableWidgetItem>
#include "node.h"
#include <QLabel>

#include "graphwidget.h"

class DNSData;
class Node;
class GraphWidget;

struct NodeWidgets {
    QTableWidgetItem *label;
    QTableWidgetItem *status;
    QTableWidgetItem *data;
};

class DetailsViewer : public QWidget
{
    Q_OBJECT
public:
    explicit DetailsViewer(Node *node, GraphWidget *graphWidget, QTabWidget *tabs = 0, QWidget *parent = 0);
    void addRow(QString recordType, DNSData *data);

signals:

public slots:
    void validateNode(QString nodeType);
    void setNode(Node *node);
    void setStatus(const DNSData *data);

private:
    Node          *m_node;
    QSignalMapper *m_mapper;
    QTabWidget    *m_tabs;
    QMap<QString, NodeWidgets *> m_rows;
    int            m_rowCount;

    QLabel        *m_title;
    QTableWidget  *m_table;
    GraphWidget   *m_graphWidget;
};

#endif // DETAILSVIEWER_H
