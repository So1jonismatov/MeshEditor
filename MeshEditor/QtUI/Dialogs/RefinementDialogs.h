#pragma once

#include <QDialog>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>

class DecimateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DecimateDialog(size_t currentFaces, QWidget *parent = nullptr);
    double getPercentage() const;

private:
    QSlider *m_slider = nullptr;
    QSpinBox *m_spinBox = nullptr;
    QLabel *m_previewLabel = nullptr;
    size_t m_currentFaces = 0;
};

class SmoothDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SmoothDialog(QWidget *parent = nullptr);
    int getPasses() const;
    float getLambda() const;

private:
    QSpinBox *m_passesSpin = nullptr;
    QDoubleSpinBox *m_lambdaSpin = nullptr;
};

class SubdivideDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SubdivideDialog(size_t currentFaces, bool hasSelection, QWidget *parent = nullptr);
    int getLevels() const;
    bool getOnlySelected() const;

private:
    QSpinBox *m_levelsSpin = nullptr;
    QCheckBox *m_selectedOnlyCheck = nullptr;
    QLabel *m_previewLabel = nullptr;
    size_t m_currentFaces = 0;
};

class WeldDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WeldDialog(QWidget *parent = nullptr);
    float getTolerance() const;

private:
    QDoubleSpinBox *m_tolSpin = nullptr;
};

class EqualizeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EqualizeDialog(float avgEdgeLength, QWidget *parent = nullptr);
    float getTargetLength() const;
    int getIterations() const;

private:
    QDoubleSpinBox *m_targetLengthSpin = nullptr;
    QSpinBox *m_iterSpin = nullptr;
};

