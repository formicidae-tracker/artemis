#pragma once

#include <GL/gl.h>

#include <string>

namespace fort {
namespace artemis {

class ShaderUtils {
public:
	static GLuint CompileProgram(const std::string & vertexCode,
	                             const std::string & fragmentCode);

	static GLuint CompileShader(const std::string & shaderCode,
	                            GLenum type);

private:

};

} // namespace artemis
} // namespace fort
