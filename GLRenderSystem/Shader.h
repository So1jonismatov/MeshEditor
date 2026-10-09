#pragma once
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glad/glad.h>

class Shader
{
private:
    unsigned int m_ID = 0;
    std::unordered_map<std::string, GLint> m_UniformLocationCache;

    static unsigned int compileShader(unsigned int type,
                                      const std::string &source,
                                      const char *stageName);

    GLint getUniformLocation(const std::string &name);

public:
    Shader(const std::string &vertexPath, const std::string &fragmentPath,
           const std::string &geometryPath = "");
    ~Shader();

    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;
    Shader(Shader &&) = delete;
    Shader &operator=(Shader &&) = delete;

    void bind() const;
    void unbind() const;

    unsigned int GetID() const;
    void SetMat4(const std::string &name, const glm::mat4 &matrix);
    void SetMat3(const std::string &name, const glm::mat3 &matrix);
    void SetMats(const glm::mat4 &world, const glm::mat4 &worldViewProj);
    void SetVec3(const std::string &name, const glm::vec3 &vec);
    void SetVec2(const std::string &name, const glm::vec2 &vec);
    void SetInt(const std::string &name, int value);
    void SetFloat(const std::string &name, float value);

    // Location-based fast path for uniforms set every draw call: resolve the
    // location once with uniformLocation(), then set by GLint — no per-call
    // string hashing. A location of -1 is silently ignored, matching GL.
    GLint uniformLocation(const std::string &name);
    void SetMat4(GLint location, const glm::mat4 &matrix);
    void SetMat3(GLint location, const glm::mat3 &matrix);
    void SetVec3(GLint location, const glm::vec3 &vec);
    void SetVec2(GLint location, const glm::vec2 &vec);
    void SetInt(GLint location, int value);
    void SetFloat(GLint location, float value);
};
