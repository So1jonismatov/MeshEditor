#include "RefinementDialogs.h"

#include <cmath>

// ── Decimate Dialog ──────────────────────────────────────────────────────────

DecimateDialog::DecimateDialog(size_t currentFaces, QWidget *parent)
    : QDialog(parent), m_currentFaces(currentFaces)
{
    setWindowTitle("Decimate Mesh");
    setFixedWidth(340);
    setStyleSheet("QDialog { background-color: #1e222b; color: #ffffff; }");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    auto *infoLabel = new QLabel(
        QString("Current Face Count: %1 faces").arg(currentFaces), this);
    infoLabel->setStyleSheet("font-size: 12px; color: #a0aec0; font-weight: 500;");
    mainLayout->addWidget(infoLabel);

    auto *sliderLayout = new QHBoxLayout;
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(5, 95);
    m_slider->setValue(50);

    m_spinBox = new QSpinBox(this);
    m_spinBox->setRange(5, 95);
    m_spinBox->setValue(50);
    m_spinBox->setSuffix(" %");
    m_spinBox->setFixedWidth(70);

    sliderLayout->addWidget(m_slider);
    sliderLayout->addWidget(m_spinBox);
    mainLayout->addLayout(sliderLayout);

    connect(m_slider, &QSlider::valueChanged, m_spinBox, &QSpinBox::setValue);
    connect(m_spinBox, QOverload<int>::of(&QSpinBox::valueChanged), m_slider, &QSlider::setValue);

    m_previewLabel = new QLabel(this);
    m_previewLabel->setStyleSheet("color: #4da6ff; font-weight: bold; font-size: 11px;");
    mainLayout->addWidget(m_previewLabel);

    auto updatePreview = [this](int val) {
        size_t estRemaining = static_cast<size_t>(m_currentFaces * (1.0 - val / 100.0));
        m_previewLabel->setText(
            QString("Target: ~%1 faces remaining (%2% reduction)")
                .arg(estRemaining)
                .arg(val));
    };
    connect(m_slider, &QSlider::valueChanged, updatePreview);
    updatePreview(50);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("Decimate");
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

double DecimateDialog::getPercentage() const
{
    return m_slider ? static_cast<double>(m_slider->value()) : 50.0;
}

// ── Smooth Dialog ────────────────────────────────────────────────────────────

SmoothDialog::SmoothDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Laplacian Smoothing");
    setFixedWidth(320);
    setStyleSheet("QDialog { background-color: #1e222b; color: #ffffff; }");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    auto *form = new QFormLayout;
    m_passesSpin = new QSpinBox(this);
    m_passesSpin->setRange(1, 25);
    m_passesSpin->setValue(3);
    m_passesSpin->setSuffix(" passes");
    form->addRow("Iterations / Passes:", m_passesSpin);

    m_lambdaSpin = new QDoubleSpinBox(this);
    m_lambdaSpin->setRange(0.05, 1.0);
    m_lambdaSpin->setSingleStep(0.05);
    m_lambdaSpin->setValue(0.50);
    m_lambdaSpin->setDecimals(2);
    form->addRow("Smoothing Factor (λ):", m_lambdaSpin);

    mainLayout->addLayout(form);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("Smooth");
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

int SmoothDialog::getPasses() const
{
    return m_passesSpin ? m_passesSpin->value() : 3;
}

float SmoothDialog::getLambda() const
{
    return m_lambdaSpin ? static_cast<float>(m_lambdaSpin->value()) : 0.5f;
}

// ── Subdivide Dialog ─────────────────────────────────────────────────────────

SubdivideDialog::SubdivideDialog(size_t currentFaces, bool hasSelection, QWidget *parent)
    : QDialog(parent), m_currentFaces(currentFaces)
{
    setWindowTitle("Subdivide Mesh");
    setFixedWidth(340);
    setStyleSheet("QDialog { background-color: #1e222b; color: #ffffff; }");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    auto *form = new QFormLayout;
    m_levelsSpin = new QSpinBox(this);
    m_levelsSpin->setRange(1, 3);
    m_levelsSpin->setValue(1);
    m_levelsSpin->setSuffix(" level(s)");
    form->addRow("Subdivision Levels:", m_levelsSpin);

    m_selectedOnlyCheck = new QCheckBox("Selected faces only", this);
    m_selectedOnlyCheck->setChecked(hasSelection);
    m_selectedOnlyCheck->setEnabled(hasSelection);
    if (!hasSelection)
        m_selectedOnlyCheck->setToolTip("No face is currently selected. Will subdivide entire mesh.");
    form->addRow("", m_selectedOnlyCheck);

    mainLayout->addLayout(form);

    m_previewLabel = new QLabel(this);
    m_previewLabel->setStyleSheet("color: #72e399; font-weight: bold; font-size: 11px;");
    mainLayout->addWidget(m_previewLabel);

    auto updatePreview = [this](int lvl) {
        size_t estCount = m_currentFaces * static_cast<size_t>(std::pow(4, lvl));
        m_previewLabel->setText(
            QString("Estimated Result: ~%1 faces (4^%2 factor)")
                .arg(estCount)
                .arg(lvl));
    };
    connect(m_levelsSpin, QOverload<int>::of(&QSpinBox::valueChanged), updatePreview);
    updatePreview(1);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("Subdivide");
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

int SubdivideDialog::getLevels() const
{
    return m_levelsSpin ? m_levelsSpin->value() : 1;
}

bool SubdivideDialog::getOnlySelected() const
{
    return m_selectedOnlyCheck ? m_selectedOnlyCheck->isChecked() : false;
}

// ── Weld Dialog ──────────────────────────────────────────────────────────────

WeldDialog::WeldDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Weld Vertices");
    setFixedWidth(300);
    setStyleSheet("QDialog { background-color: #1e222b; color: #ffffff; }");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    auto *form = new QFormLayout;
    m_tolSpin = new QDoubleSpinBox(this);
    m_tolSpin->setRange(0.00001, 1.0);
    m_tolSpin->setSingleStep(0.001);
    m_tolSpin->setValue(0.001);
    m_tolSpin->setDecimals(5);
    form->addRow("Snap Distance (ε):", m_tolSpin);

    mainLayout->addLayout(form);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("Weld");
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

float WeldDialog::getTolerance() const
{
    return m_tolSpin ? static_cast<float>(m_tolSpin->value()) : 0.001f;
}

// ── Equalize Triangles Dialog ───────────────────────────────────────────────

EqualizeDialog::EqualizeDialog(float avgEdgeLength, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Equalize Triangles (Isotropic Remesh)");
    setFixedWidth(340);
    setStyleSheet("QDialog { background-color: #1e222b; color: #ffffff; }");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    auto *descLabel = new QLabel("Make all mesh triangles have uniform equal sizes by splitting long edges, collapsing short edges, and tangential relaxation.", this);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("color: #a0aec0; font-size: 11px;");
    mainLayout->addWidget(descLabel);

    auto *form = new QFormLayout;
    m_targetLengthSpin = new QDoubleSpinBox(this);
    m_targetLengthSpin->setRange(0.0001, 1000.0);
    m_targetLengthSpin->setDecimals(4);
    m_targetLengthSpin->setSingleStep(avgEdgeLength * 0.1 > 0.001 ? avgEdgeLength * 0.1 : 0.01);
    m_targetLengthSpin->setValue(avgEdgeLength > 0.0f ? avgEdgeLength : 0.1f);
    m_targetLengthSpin->setSuffix(" units");
    form->addRow("Target Edge Length:", m_targetLengthSpin);

    m_iterSpin = new QSpinBox(this);
    m_iterSpin->setRange(1, 10);
    m_iterSpin->setValue(3);
    m_iterSpin->setSuffix(" iterations");
    form->addRow("Optimization Passes:", m_iterSpin);

    mainLayout->addLayout(form);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("Equalize");
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

float EqualizeDialog::getTargetLength() const
{
    return m_targetLengthSpin ? static_cast<float>(m_targetLengthSpin->value()) : 0.1f;
}

int EqualizeDialog::getIterations() const
{
    return m_iterSpin ? m_iterSpin->value() : 3;
}
