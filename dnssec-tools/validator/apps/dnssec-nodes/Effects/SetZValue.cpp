#include "SetZValue.h"

#include "node.h"

SetZValue::SetZValue(unsigned int zvalue)
    : m_zvalue(zvalue)
{
}

void SetZValue::applyToNode(Node *node)
{
    node->setZValue(m_zvalue);
}

void SetZValue::resetNode(Node *node)
{
    node->setZValue(-1);
}
