#pragma once

#include "Operator.h"
#include "Keys.h"

#include <vector>
#include <stack>
#include <map>
#include <memory>
#include <functional>
#include <concepts>
#include <stdexcept>

class View;

class OperatorDispatcher
{
public:
    friend class View;
    using OperatorLambda = std::function<void(View &, Action, Modifier)>;

    void addOperator(KeyCode enterKey, KeyCode exitKey,
                     std::unique_ptr<Operator> op);
    void addOperator(ButtonCode button, std::unique_ptr<Operator> op);
    void addOperator(KeyCode key, std::unique_ptr<Operator> op);
    void addOperator(KeyCode key, OperatorLambda lambda);

    bool hasActiveExitKey(KeyCode key) const;
    // True while the enter/exit operator registered under `enterKey` is
    // active. Lets the GUI mirror tool state instead of tracking it itself.
    bool isOperatorActive(KeyCode enterKey) const;
    void resetActiveOperators(View &view);

private:
    void processMouseInput(View &view, ButtonCode button, Action action,
                           Modifier mods, double x, double y);
    void processMouseMove(View &view, double x, double y);
    void processKeyboardInput(View &view, KeyCode key, Action action,
                              Modifier mods);
    void processUpdate(View &view);

    void checkKey(KeyCode key, bool allowSharedExitKey = false) const;
    void checkButton(ButtonCode button) const;

    std::map<KeyCode, std::unique_ptr<Operator>> m_keyOperators;
    std::map<ButtonCode, std::unique_ptr<Operator>> m_buttonOperators;
    std::map<KeyCode, std::pair<KeyCode, std::unique_ptr<Operator>>>
        m_enterExitOperators;
    std::map<KeyCode, OperatorLambda> m_lambdas;

    std::map<ButtonCode, Operator *> m_activeButtonOps;
    std::map<KeyCode, Operator *> m_activeKeyOps;
    std::map<KeyCode, Operator *> m_activeEnterExitOps;
};