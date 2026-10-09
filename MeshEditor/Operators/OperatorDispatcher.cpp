#include "OperatorDispatcher.h"
#include "View.h"
#include "Topology/MoveOrthogonalFaces.h"

void OperatorDispatcher::addOperator(KeyCode enterKey, KeyCode exitKey,
                                     std::unique_ptr<Operator> op)
{
    checkKey(enterKey);
    checkKey(exitKey, true);
    m_enterExitOperators[enterKey] = std::make_pair(exitKey, std::move(op));
}

void OperatorDispatcher::addOperator(ButtonCode button,
                                     std::unique_ptr<Operator> op)
{
    checkButton(button);
    m_buttonOperators[button] = std::move(op);
}

void OperatorDispatcher::addOperator(KeyCode key, std::unique_ptr<Operator> op)
{
    checkKey(key);
    m_keyOperators[key] = std::move(op);
}

void OperatorDispatcher::addOperator(KeyCode key, OperatorLambda lambda)
{
    checkKey(key);
    m_lambdas[key] = std::move(lambda);
}

bool OperatorDispatcher::isOperatorActive(KeyCode enterKey) const
{
    return m_activeEnterExitOps.find(enterKey) != m_activeEnterExitOps.end();
}

bool OperatorDispatcher::hasActiveExitKey(KeyCode key) const
{
    for (const auto &pair : m_activeEnterExitOps)
    {
        KeyCode enterKey = pair.first;
        auto it = m_enterExitOperators.find(enterKey);
        if (it != m_enterExitOperators.end() && it->second.first == key)
        {
            return true;
        }
    }
    return false;
}

void OperatorDispatcher::resetActiveOperators(View &view)
{
    for (auto &[button, op] : m_activeButtonOps)
    {
        if (op)
            op->onExit(view);
    }
    for (auto &[key, op] : m_activeKeyOps)
    {
        if (op)
            op->onExit(view);
    }
    for (auto &[key, op] : m_activeEnterExitOps)
    {
        if (op)
            op->onExit(view);
    }

    m_activeButtonOps.clear();
    m_activeKeyOps.clear();
    m_activeEnterExitOps.clear();
}

void OperatorDispatcher::checkKey(KeyCode key, bool allowSharedExitKey) const
{
    if (m_keyOperators.count(key) > 0)
        throw std::logic_error("Key already bound to a single-key operator");
    if (m_lambdas.count(key) > 0)
        throw std::logic_error("Key already bound to a lambda operator");
    if (m_enterExitOperators.count(key) > 0)
        throw std::logic_error(
            "Key already bound as an enter key of an enter-exit operator");

    if (!allowSharedExitKey)
    {
        for (const auto &pair : m_enterExitOperators)
        {
            if (pair.second.first == key)
                throw std::logic_error("Key already bound as an exit key of an "
                                       "enter-exit operator");
        }
    }
}

void OperatorDispatcher::checkButton(ButtonCode button) const
{
    if (m_buttonOperators.count(button) > 0)
        throw std::logic_error("Button already bound to an operator");
}

void OperatorDispatcher::processMouseInput(View &view, ButtonCode button,
                                           Action action, Modifier mods,
                                           double x, double y)
{
    // An active modal operator that claims this mouse button gets it exclusively
    // (e.g. measurement tools claim Left Click to override Pan)
    for (const auto &[enterKey, activeOp] : m_activeEnterExitOps)
    {
        if (activeOp->consumesMouseInput(button))
        {
            activeOp->onMouseInput(view, button, action, mods, x, y);
            return;
        }
    }

    auto it = m_buttonOperators.find(button);
    if (it != m_buttonOperators.end())
    {
        Operator *op = it->second.get();
        if (action == Action::Press)
        {
            if (m_activeButtonOps.find(button) == m_activeButtonOps.end())
            {
                m_activeButtonOps[button] = op;
                op->onEnter(view);
            }
            op->onMouseInput(view, button, action, mods, x, y);
        }
        else if (action == Action::Release)
        {
            if (m_activeButtonOps.find(button) != m_activeButtonOps.end())
            {
                op->onMouseInput(view, button, action, mods, x, y);
                op->onExit(view);
                m_activeButtonOps.erase(button);
            }
        }
    }

    std::vector<Operator *> otherOps;
    for (const auto &[btn, op] : m_activeButtonOps)
    {
        if (btn != button)
            otherOps.push_back(op);
    }
    for (const auto &[k, op] : m_activeKeyOps)
    {
        otherOps.push_back(op);
    }
    for (const auto &[k, op] : m_activeEnterExitOps)
    {
        otherOps.push_back(op);
    }

    for (Operator *op : otherOps)
    {
        op->onMouseInput(view, button, action, mods, x, y);
    }
}

void OperatorDispatcher::processMouseMove(View &view, double x, double y)
{
    std::vector<Operator *> activeOps;
    for (const auto &[btn, op] : m_activeButtonOps)
    {
        activeOps.push_back(op);
    }
    for (const auto &[k, op] : m_activeKeyOps)
    {
        activeOps.push_back(op);
    }
    for (const auto &[k, op] : m_activeEnterExitOps)
    {
        activeOps.push_back(op);
    }

    for (Operator *op : activeOps)
    {
        op->onMouseMove(view, x, y);
    }
}

void OperatorDispatcher::processKeyboardInput(View &view, KeyCode key,
                                              Action action, Modifier mods)
{
    if (action == Action::Press)
    {
        auto it = m_lambdas.find(key);
        if (it != m_lambdas.end())
        {
            it->second(view, action, mods);
            return;
        }
    }

    if (action == Action::Press)
    {
        auto it = m_enterExitOperators.find(key);
        if (it != m_enterExitOperators.end())
        {
            Operator *op = it->second.second.get();
            auto activeIt = m_activeEnterExitOps.find(key);
            if (activeIt != m_activeEnterExitOps.end() &&
                it->second.first == key)
            {
                op->onKeyboardInput(view, key, action, mods);
                op->onExit(view);
                m_activeEnterExitOps.erase(activeIt);
                return;
            }

            if (activeIt == m_activeEnterExitOps.end())
            {
                // Exclusive tools (manipulator gizmos) cannot coexist: exit
                // any other active exclusive tool before entering this one.
                if (op->isExclusiveTool())
                {
                    for (auto other = m_activeEnterExitOps.begin();
                         other != m_activeEnterExitOps.end();)
                    {
                        if (other->first != key &&
                            other->second->isExclusiveTool())
                        {
                            other->second->onExit(view);
                            other = m_activeEnterExitOps.erase(other);
                        }
                        else
                        {
                            ++other;
                        }
                    }
                }
                m_activeEnterExitOps[key] = op;
                op->onEnter(view);
            }
            op->onKeyboardInput(view, key, action, mods);
            return;
        }

        for (auto activeIt = m_activeEnterExitOps.begin();
             activeIt != m_activeEnterExitOps.end();)
        {
            KeyCode enterKey = activeIt->first;
            Operator *op = activeIt->second;
            KeyCode exitKey = m_enterExitOperators[enterKey].first;

            if (exitKey == key)
            {
                op->onKeyboardInput(view, key, action, mods);
                op->onExit(view);
                activeIt = m_activeEnterExitOps.erase(activeIt);
            }
            else
            {
                ++activeIt;
            }
        }
    }

    // An active modal operator that claims this key (FPS mode claims the
    // arrow keys for movement) gets it exclusively — press, repeat AND
    // release — bypassing any globally bound single-key operator.
    for (const auto &[enterKey, activeOp] : m_activeEnterExitOps)
    {
        if (activeOp->consumesKey(key))
        {
            activeOp->onKeyboardInput(view, key, action, mods);
            return;
        }
    }

    auto itKey = m_keyOperators.find(key);
    if (itKey != m_keyOperators.end())
    {
        Operator *op = itKey->second.get();
        const bool isPressLike =
            action == Action::Press || action == Action::Repeat;
        if (isPressLike)
        {
            if (m_activeKeyOps.find(key) == m_activeKeyOps.end())
            {
                m_activeKeyOps[key] = op;
                op->onEnter(view);
            }
            op->onKeyboardInput(view, key, action, mods);
        }
        else if (action == Action::Release)
        {
            if (m_activeKeyOps.find(key) != m_activeKeyOps.end())
            {
                op->onKeyboardInput(view, key, action, mods);
                op->onExit(view);
                m_activeKeyOps.erase(key);
            }
        }
        return;
    }

    std::vector<Operator *> otherOps;
    for (const auto &[btn, op] : m_activeButtonOps)
    {
        otherOps.push_back(op);
    }
    for (const auto &[k, op] : m_activeKeyOps)
    {
        if (k != key)
            otherOps.push_back(op);
    }
    for (const auto &[k, op] : m_activeEnterExitOps)
    {
        otherOps.push_back(op);
    }

    for (Operator *op : otherOps)
    {
        op->onKeyboardInput(view, key, action, mods);
    }
}

void OperatorDispatcher::processUpdate(View &view)
{
    std::vector<Operator *> activeOps;
    for (const auto &[btn, op] : m_activeButtonOps)
    {
        activeOps.push_back(op);
    }
    for (const auto &[k, op] : m_activeKeyOps)
    {
        activeOps.push_back(op);
    }
    for (const auto &[k, op] : m_activeEnterExitOps)
    {
        activeOps.push_back(op);
    }

    for (Operator *op : activeOps)
    {
        op->onUpdate(view);
    }
}