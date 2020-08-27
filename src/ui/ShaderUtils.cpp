#include <GL/glew.h>


#include <vector>
#include <stdexcept>

#include "ShaderUtils.hpp"

namespace fort {
namespace artemis {

#define get_compilation_error(type,sType,psID) do {	  \
		GLint result; \
		int infoLength; \
		glGet ## type ##iv(psID,sType,&result); \
		glGet ## type ##iv(psID,GL_INFO_LOG_LENGTH,&infoLength); \
		if ( infoLength > 0 ) { \
			std::vector<char> message(infoLength + 1); \
			glGet ## type ## InfoLog(psID,infoLength,NULL,&message[0]); \
			throw std::runtime_error("Could not compile " #type ": " + std::string(message.begin(),message.end())); \
		} \
	} while(0)

#define get_shader_compilation_error(psID) get_compilation_error(Shader,GL_COMPILE_STATUS,psID)
#define get_program_compilation_error(psID) get_compilation_error(Program,GL_LINK_STATUS,psID)

GLuint ShaderUtils::CompileShader(const std::string & shaderCode, GLenum type) {
	GLuint shaderID = glCreateShader(type);
	GLint result = GL_FALSE;
	int infoLength;

	const char * code = shaderCode.c_str();
	glShaderSource(shaderID,1,&code,NULL);
	glCompileShader(shaderID);

	get_shader_compilation_error(shaderID);

	return shaderID;
}

GLuint ShaderUtils::CompileProgram(const std::string &vertexShaderCode,
                                   const std::string &fragmentShaderCode){

	// Create the shaders
	GLuint vertexShaderID = CompileShader(vertexShaderCode,GL_VERTEX_SHADER);
	GLuint fragmentShaderID = CompileShader(fragmentShaderCode,GL_FRAGMENT_SHADER);


	GLuint programID = glCreateProgram();
	glAttachShader(programID, vertexShaderID);
	glAttachShader(programID, fragmentShaderID);
	glLinkProgram(programID);

	get_program_compilation_error(programID);

	glDetachShader(programID, vertexShaderID);
	glDetachShader(programID, fragmentShaderID);

	glDeleteShader(vertexShaderID);
	glDeleteShader(fragmentShaderID);

	return programID;
}

} // namespace artemis
} // namespace fort
