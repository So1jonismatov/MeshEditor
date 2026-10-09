#pragma once

#include "Manipulator.h"
#include "TranslationManipulator.h"
#include "RotationManipulator.h"
#include "ScaleManipulator.h"
#include <memory>

class Triad : public Manipulator
{
public:
    Triad(float majorRadius = 1.0f, float arrowLength = 1.5f);
};

class ScaleTriad : public Manipulator
{
public:
    ScaleTriad(float arrowLength = 1.5f);
};