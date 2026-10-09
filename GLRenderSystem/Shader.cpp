#include "Shader.h"
#include <iostream>

#include "utils/ShaderReader.h"

Shader::Shader(const std::string &vertexPath, const std::string &fragmentPath,
               const std::string &geometryPath)
{
    const std::string vertexSource = Utils::readSrc(vertexPath);
    const std::string fragmentSource = Utils::readSrc(fragmentPath);

    if (vertexSource.empty() || fragmentSource.empty())
        return;

    const unsigned int vertexShader =
        compileShader(GL_VERTEX_SHADER, vertexSource, "Vertex");
    if (vertexShader == 0)
        return;

    const unsigned int fragmentShader =
        compileShader(GL_FRAGMENT_SHADER, fragmentSource, "Fragment");
    if (fragmentShader == 0)
    {
        glDeleteShader(vertexShader);
        return;
    }

    // Geometry stage is optional: line/immediate shaders have none, while the
    // black-edge mesh shader supplies one.
    unsigned int geometryShader = 0;
    if (!geometryPath.empty())
    {
        const std::string geometrySource = Utils::readSrc(geometryPath);
        if (!geometrySource.empty())
        {
            geometryShader =
                compileShader(GL_GEOMETRY_SHADER, geometrySource, "Geometry");
        }
    }

    m_ID = glCreateProgram();
    glAttachShader(m_ID, vertexShader);
    if (geometryShader != 0)
    {
        glAttachShader(m_ID, geometryShader);
    }
    glAttachShader(m_ID, fragmentShader);
    glLinkProgram(m_ID);

    int success = 0;
    char infoLog[512];
    glGetProgramiv(m_ID, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(m_ID, 512, nullptr, infoLog);
        std::cerr << "Shader Program Linking Failed:\n" << infoLog << std::endl;
        glDeleteProgram(m_ID);
        m_ID = 0;
    }

    glDeleteShader(vertexShader);
    if (geometryShader != 0)
    {
        glDeleteShader(geometryShader);
    }
    glDeleteShader(fragmentShader);
}

Shader::~Shader()
{
    glDeleteProgram(m_ID);
}

unsigned int Shader::compileShader(unsigned int type, const std::string &source,
                                   const char *stageName)
{
    unsigned int shader = glCreateShader(type);
    const char *sourcePtr = source.c_str();
    glShaderSource(shader, 1, &sourcePtr, nullptr);
    glCompileShader(shader);

    int success = 0;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << stageName << " Shader Compilation Failed:\n"
                  << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

//------------------------------------------------------
//
// Getter and Setters
//
//-------------------------------------------------------

unsigned int Shader::GetID() const
{
    return m_ID;
}

GLint Shader::getUniformLocation(const std::string &name)
{
    const auto it = m_UniformLocationCache.find(name);
    if (it != m_UniformLocationCache.end())
        return it->second;

    const GLint location = glGetUniformLocation(m_ID, name.c_str());
    m_UniformLocationCache.emplace(name, location);
    return location;
}

void Shader::SetMat4(const std::string &name, const glm::mat4 &matrix)
{
    if (m_ID == 0)
        return;

    const GLint location = getUniformLocation(name);
    if (location < 0)
        return;

    glProgramUniformMatrix4fv(m_ID, location, 1, GL_FALSE,
                              glm::value_ptr(matrix));
}

void Shader::SetMat3(const std::string &name, const glm::mat3 &matrix)
{
    if (m_ID == 0)
        return;

    const GLint location = getUniformLocation(name);
    if (location < 0)
        return;

    glProgramUniformMatrix3fv(m_ID, location, 1, GL_FALSE,
                              glm::value_ptr(matrix));
}

void Shader::SetMats(const glm::mat4 &world, const glm::mat4 &worldViewProj)
{
    SetMat4("world", world);
    SetMat4("worldViewProj", worldViewProj);
}

void Shader::SetVec3(const std::string &name, const glm::vec3 &vec)
{
    if (m_ID == 0)
        return;

    const GLint location = getUniformLocation(name);
    if (location < 0)
        return;

    glProgramUniform3fv(m_ID, location, 1, glm::value_ptr(vec));
}

void Shader::SetInt(const std::string &name, int value)
{
    if (m_ID == 0)
        return;

    const GLint location = getUniformLocation(name);
    if (location < 0)
        return;

    glProgramUniform1i(m_ID, location, value);
}

void Shader::bind() const
{
    if (m_ID == 0)
        return;

    glUseProgram(m_ID);
}

void Shader::unbind() const
{
    glUseProgram(0);
}

void Shader::SetFloat(const std::string &name, float value)
{
    if (m_ID == 0)
        return;

    const GLint location = getUniformLocation(name);
    if (location < 0)
        return;

    glProgramUniform1f(m_ID, location, value);
}

GLint Shader::uniformLocation(const std::string &name)
{
    if (m_ID == 0)
        return -1;
    return getUniformLocation(name);
}

void Shader::SetMat4(GLint location, const glm::mat4 &matrix)
{
    if (m_ID == 0 || location < 0)
        return;
    glProgramUniformMatrix4fv(m_ID, location, 1, GL_FALSE,
                              glm::value_ptr(matrix));
}

void Shader::SetMat3(GLint location, const glm::mat3 &matrix)
{
    if (m_ID == 0 || location < 0)
        return;
    glProgramUniformMatrix3fv(m_ID, location, 1, GL_FALSE,
                              glm::value_ptr(matrix));
}

void Shader::SetVec3(GLint location, const glm::vec3 &vec)
{
    if (m_ID == 0 || location < 0)
        return;
    glProgramUniform3fv(m_ID, location, 1, glm::value_ptr(vec));
}

void Shader::SetInt(GLint location, int value)
{
    if (m_ID == 0 || location < 0)
        return;
    glProgramUniform1i(m_ID, location, value);
}

void Shader::SetFloat(GLint location, float value)
{
    if (m_ID == 0 || location < 0)
        return;
    glProgramUniform1f(m_ID, location, value);
}

void Shader::SetVec2(const std::string &name, const glm::vec2 &vec)
{
    if (m_ID == 0) return;
    const GLint location = getUniformLocation(name);
    if (location < 0) return;
    glProgramUniform2fv(m_ID, location, 1, glm::value_ptr(vec));
}

void Shader::SetVec2(GLint location, const glm::vec2 &vec)
{
    if (m_ID == 0 || location < 0) return;
    glProgramUniform2fv(m_ID, location, 1, glm::value_ptr(vec));
}
