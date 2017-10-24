#include "Compositor.h"

#define RETURN_ERR(T) {Compositor::m_lastError=(T);return false;}
#define RETURN_OK()	  {Compositor::m_lastError=NONE;return true;}

///
/// \brief Constructor.
/// Constructor.
///
Compositor::Compositor()
{
//	glGenFramebuffers(1, &m_fboID);

	initializeVertexShader();
	initializeBufferObject();
	setResolution(512, 512);
	m_passes.clear();
	m_pipelines.clear();

}

///
/// \brief Destructor.
/// Destructor.
///
Compositor::~Compositor()
{
	glDeleteShader(m_shaderVertex);
	while (m_passes.size() > 0)
		deletePass(m_passes.begin()->first);

	glDeleteBuffers(1, &m_vertexBuffer);
	glDeleteVertexArrays(1, &m_vertexArray);

//	glDeleteFramebuffers(1, &m_fboID);
}

///
/// \brief To return latest error.
/// To return latest error. I am not sure if we should also handle OpenGL errors.
///
Compositor::error Compositor::getLastError()
{ 
	error toReturn = m_lastError;
	m_lastError = Compositor::NONE;
	return toReturn;
}

///
/// \brief To create a new pass.
/// To To create a new pass.
///
void Compositor::setResolution(int w, int h)
{
	m_width = w; 
	m_height = h;
	m_lastError = Compositor::NONE;
}

///
/// \brief To create a new pass.
/// To To create a new pass.
///
int Compositor::createNewPass()
{
	static int passID = -1;
	GLuint fboID;
	pass newPass;

	++passID;

	newPass.initialized = false;
	newPass.texInputs.clear();
	newPass.texOutputs.clear();
	newPass.texOutputsChannels = nullptr;
	newPass.shaderFragment = 0;
	newPass.shaderProgram = 0;
	glGenFramebuffers(1, &newPass.fbo);
	m_passes[passID] = newPass;

	m_lastError = Compositor::NONE;
	return passID;
}

///
/// \brief To load fragment shader to be used for doing compositing.
/// To load fragment shader to be used for doing compositing.
///
bool Compositor::loadShader(int passID, char* filename)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) { m_lastError = Compositor::PASS_NOT_FOUND;  return false; }
	else
	{
		//parts of this are taken from http://www.opengl-tutorial.org/beginners-tutorials/tutorial-2-the-first-triangle/
		GLuint newShader = glCreateShader(GL_FRAGMENT_SHADER);
		std::string FragmentShaderCode;
		std::ifstream FragmentShaderStream(filename, std::ios::in);
		if (FragmentShaderStream.is_open())
		{
			std::string Line = "";
			while (getline(FragmentShaderStream, Line))
				FragmentShaderCode += "\n" + Line;
			FragmentShaderStream.close();
		}
		else RETURN_ERR(SHADER_FILE_NOT_FOUND);

		char const * FragmentSourcePointer = FragmentShaderCode.c_str();
		glShaderSource(newShader, 1, &FragmentSourcePointer, NULL);
		glCompileShader(newShader);

		GLint Result;
		glGetShaderiv(newShader, GL_COMPILE_STATUS, &Result);
		if (Result == GL_FALSE) RETURN_ERR(Compositor::SHADER_COMPILE_FAIL)

		GLuint newProgram = glCreateProgram();
		glAttachShader(newProgram, m_shaderVertex);
		glAttachShader(newProgram, newShader);
		glLinkProgram(newProgram);
		glGetProgramiv(newProgram, GL_LINK_STATUS, &Result);
		if (Result == GL_FALSE)
		{
			glDeleteProgram(newProgram);
			RETURN_ERR(Compositor::SHADER_LINKING_FAIL)
		}

		//need to bind the VAO and array buffer to the vPos in the vertex shader
		GLint bufferVertexArray, bufferArrayBuffer;
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &bufferVertexArray);
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &bufferArrayBuffer);

		glBindVertexArray(m_vertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);

		GLint uni_vpos = -1;
		uni_vpos = glGetAttribLocation(newProgram, "vPos");
		glVertexAttribPointer(uni_vpos, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glEnableVertexAttribArray(uni_vpos);

		glBindVertexArray(bufferVertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, bufferArrayBuffer);

		p->second.shaderFragment = newShader;
		p->second.shaderProgram = newProgram;
		p->second.initialized = true;

		RETURN_OK()
	}
}

///
/// \brief To delete new pass.
/// To delete a pass.
///
bool Compositor::deletePass(int passID)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		//clear everything inside p->second here
		glDetachShader(p->second.shaderProgram, m_shaderVertex);
		glDetachShader(p->second.shaderProgram, p->second.shaderFragment);
		glDeleteShader(p->second.shaderFragment);
		glDeleteProgram(p->second.shaderProgram);
		glDeleteFramebuffers(1, &p->second.fbo);
		if (p->second.texOutputsChannels != nullptr) delete[] p->second.texOutputsChannels;
		//finally delete the pass here
		m_passes.erase(p);
	}
	RETURN_OK()
}

///
/// \brief Render the specified pass.
/// Render the specified pass.
///
bool Compositor::renderPass(int passID)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		if (p->second.initialized == false) RETURN_ERR(Compositor::PASS_PROGRAM_NOT_INITIALIZED)
		if (p->second.texOutputs.size() == 0) RETURN_ERR(Compositor::PASS_OUTPUT_NOT_FOUND)

		pushState();

		renderPassInternal(p);

		popState();
	}
	RETURN_OK()
}

///
/// \brief To create a new pipeline, which is a list of sequential passes.
/// To To create a new pipeline, which is a list of sequential passes.
///
int Compositor::createSequentialPipeline()
{
	static int seqID = -1;
	++seqID;

	std::vector<int> emptyPasses;
	emptyPasses.clear();
	m_pipelines[seqID] = emptyPasses;

	m_lastError = Compositor::NONE;
	return seqID;
}

///
/// \brief To set passes of a pipeline.
/// To set passes of a pipeline.
///
bool Compositor::setPipeline(int id, std::vector<int> inputPasses)
{
	std::map<int, std::vector<int>>::iterator p = m_pipelines.find(id);
	if (p == m_pipelines.end()) RETURN_ERR(Compositor::PIPELINE_NOT_FOUND)
	else
	{
		if (!verifyPipeline(inputPasses)) RETURN_ERR(Compositor::PIPELINE_NOT_COMPLETE)
		m_pipelines[id] = inputPasses;
	}
	RETURN_OK()
}

///
/// \brief To get all the passes index from a pipeline.
/// To get all the passes index from a pipeline.
///
std::vector<int> Compositor::getPipeline(int id)
{
	std::vector<int> passes;
	passes.clear();
	std::map<int, std::vector<int>>::iterator p = m_pipelines.find(id);
	if (p != m_pipelines.end())
	{
		passes = p->second;
		m_lastError = Compositor::PIPELINE_NOT_FOUND;
	}
	return passes;
}

///
/// \brief To delete a pipeline.
/// To delete a pipeline.
///
bool Compositor::deletePipeline(int id)
{
	std::map<int, std::vector<int>>::iterator p = m_pipelines.find(id);
	if (p == m_pipelines.end()) RETURN_ERR(Compositor::PIPELINE_NOT_FOUND)
	else
		m_pipelines.erase(p);

	RETURN_OK()
}

///
/// \brief To render a pipeline.
/// To render a pipeline by sequentially render the passes.
///
bool Compositor::renderPipeline(int id)
{
	std::map<int, std::vector<int>>::iterator p = m_pipelines.find(id);
	if (p == m_pipelines.end()) RETURN_ERR(Compositor::PIPELINE_NOT_FOUND)
	else
	{
		if (!verifyPipeline(p->second)) RETURN_ERR(Compositor::PIPELINE_NOT_COMPLETE)
		pushState();
		for (int i = 0; i < p->second.size(); i++)
		{
			std::map<int, pass>::iterator p2 = m_passes.find(p->second[i]);
			renderPassInternal(p2);
		}
		popState();
	}

	RETURN_OK()
}

///
/// \brief To set 1D uniform value.
/// To set 1D uniform value
///
bool Compositor::setUniformValue1f(int passID, char* uniName, GLfloat v0)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);
		GLint loc = glGetUniformLocation(p->second.shaderProgram, uniName);
		glUniform1f(loc, v0);
		glUseProgram(currentProg);
	}

	RETURN_OK()
}

///
/// \brief To set 2D uniform value.
/// To set 2D uniform value
///
bool Compositor::setUniformValue2f(int passID, char* uniName, GLfloat v0, GLfloat v1)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);
		GLint loc = glGetUniformLocation(p->second.shaderProgram, uniName);
		glUniform2f(loc, v0, v1);
		glUseProgram(currentProg);
	}

	RETURN_OK()
}

///
/// \brief To set 3D uniform value.
/// To set 3D uniform value
///
bool Compositor::setUniformValue3f(int passID, char* uniName, GLfloat v0, GLfloat v1, GLfloat v2)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);
		GLint loc = glGetUniformLocation(p->second.shaderProgram, uniName);
		glUniform3f(loc, v0, v1, v2);
		glUseProgram(currentProg);
	}

	RETURN_OK()
}

///
/// \brief To set 4D uniform value.
/// To set 4D uniform value
///
bool Compositor::setUniformValue4f(int passID, char* uniName, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);
		GLint loc = glGetUniformLocation(p->second.shaderProgram, uniName);
		glUniform4f(loc, v0, v1, v2, v3);
		glUseProgram(currentProg);
	}

	RETURN_OK()
}

///
/// \brief To set 1D uniform value.
/// To set 1D uniform value
///
bool Compositor::setUniformValue1i(int passID, char* uniName, GLint v0)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);
		GLint loc = glGetUniformLocation(p->second.shaderProgram, uniName);
		glUniform1i(loc, v0);
		glUseProgram(currentProg);
	}

	RETURN_OK()
}

///
/// \brief To set 2D uniform value.
/// To set 2D uniform value
///
bool Compositor::setUniformValue2i(int passID, char* uniName, GLint v0, GLint v1)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);
		GLint loc = glGetUniformLocation(p->second.shaderProgram, uniName);
		glUniform2i(loc, v0, v1);
		glUseProgram(currentProg);
	}

	RETURN_OK()
}

///
/// \brief To set 3D uniform value.
/// To set 3D uniform value
///
bool Compositor::setUniformValue3i(int passID, char* uniName, GLint v0, GLint v1, GLint v2)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);
		GLint loc = glGetUniformLocation(p->second.shaderProgram, uniName);
		glUniform3i(loc, v0, v1, v2);
		glUseProgram(currentProg);
	}

	RETURN_OK()
}

///
/// \brief To set 4D uniform value.
/// To set 4D uniform value
///
bool Compositor::setUniformValue4i(int passID, char* uniName, GLint v0, GLint v1, GLint v2, GLint v3)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);
		GLint loc = glGetUniformLocation(p->second.shaderProgram, uniName);
		glUniform4i(loc, v0, v1, v2, v3);
		glUseProgram(currentProg);
	}

	RETURN_OK()
}

///
/// \brief To set 1D uniform value.
/// To set 1D uniform value
///
bool Compositor::setUniformValue1ui(int passID, char* uniName, GLuint v0)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);
		GLint loc = glGetUniformLocation(p->second.shaderProgram, uniName);
		glUniform1ui(loc, v0);
		glUseProgram(currentProg);
	}

	RETURN_OK()
}

///
/// \brief To set 2D uniform value.
/// To set 2D uniform value
///
bool Compositor::setUniformValue2ui(int passID, char* uniName, GLuint v0, GLuint v1)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);
		GLint loc = glGetUniformLocation(p->second.shaderProgram, uniName);
		glUniform2ui(loc, v0, v1);
		glUseProgram(currentProg);
	}

	RETURN_OK()
}

///
/// \brief To set 3D uniform value.
/// To set 3D uniform value
///
bool Compositor::setUniformValue3ui(int passID, char* uniName, GLuint v0, GLuint v1, GLuint v2)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);
		GLint loc = glGetUniformLocation(p->second.shaderProgram, uniName);
		glUniform3ui(loc, v0, v1, v2);
		glUseProgram(currentProg);
	}

	RETURN_OK()
}

///
/// \brief To set 4D uniform value.
/// To set 4D uniform value
///
bool Compositor::setUniformValue4ui(int passID, char* uniName, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);
		GLint loc = glGetUniformLocation(p->second.shaderProgram, uniName);
		glUniform4ui(loc, v0, v1, v2, v3);
		glUseProgram(currentProg);
	}

	RETURN_OK()
}

///
/// \brief To set 1D uniform value.
/// To set 1D uniform value
///
bool Compositor::setUniformValue1fv(int passID, char* uniName, GLfloat *v)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);
		GLint loc = glGetUniformLocation(p->second.shaderProgram, uniName);
		glUniform1fv(loc, 1, v);
		glUseProgram(currentProg);
	}

	RETURN_OK()
}

///
/// \brief To set 2D uniform value.
/// To set 2D uniform value
///
bool Compositor::setUniformValue2fv(int passID, char* uniName, GLfloat *v)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);
		GLint loc = glGetUniformLocation(p->second.shaderProgram, uniName);
		glUniform2fv(loc, 2, v);
		glUseProgram(currentProg);
	}

	RETURN_OK()
}

///
/// \brief To set 3D uniform value.
/// To set 3D uniform value
///
bool Compositor::setUniformValue3fv(int passID, char* uniName, GLfloat *v)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);
		GLint loc = glGetUniformLocation(p->second.shaderProgram, uniName);
		glUniform3fv(loc, 3, v);
		glUseProgram(currentProg);
	}

	RETURN_OK()
}

///
/// \brief To set 4D uniform value.
/// To set 4D uniform value
///
bool Compositor::setUniformValue4fv(int passID, char* uniName, GLfloat *v)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);
		GLint loc = glGetUniformLocation(p->second.shaderProgram, uniName);
		glUniform4fv(loc, 4, v);
		glUseProgram(currentProg);
	}

	RETURN_OK()
}

///
/// \brief To set 1D uniform value.
/// To set 1D uniform value
///
bool Compositor::setUniformValue1iv(int passID, char* uniName, GLint *v)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);
		GLint loc = glGetUniformLocation(p->second.shaderProgram, uniName);
		glUniform1iv(loc, 1, v);
		glUseProgram(currentProg);
	}

	RETURN_OK()
}

///
/// \brief To set 2D uniform value.
/// To set 2D uniform value
///
bool Compositor::setUniformValue2iv(int passID, char* uniName, GLint *v)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);
		GLint loc = glGetUniformLocation(p->second.shaderProgram, uniName);
		glUniform2iv(loc, 2, v);
		glUseProgram(currentProg);
	}

	RETURN_OK()
}

///
/// \brief To set 3D uniform value.
/// To set 3D uniform value
///
bool Compositor::setUniformValue3iv(int passID, char* uniName, GLint *v)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);
		GLint loc = glGetUniformLocation(p->second.shaderProgram, uniName);
		glUniform3iv(loc, 3, v);
		glUseProgram(currentProg);
	}

	RETURN_OK()
}

///
/// \brief To set 4D uniform value.
/// To set 4D uniform value
///
bool Compositor::setUniformValue4iv(int passID, char* uniName, GLint *v)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);
		GLint loc = glGetUniformLocation(p->second.shaderProgram, uniName);
		glUniform4iv(loc, 4, v);
		glUseProgram(currentProg);
	}

	RETURN_OK()
}

///
/// \brief To set 1D uniform value.
/// To set 1D uniform value
///
bool Compositor::setUniformValue1uiv(int passID, char* uniName, GLuint *v)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);
		GLint loc = glGetUniformLocation(p->second.shaderProgram, uniName);
		glUniform1uiv(loc, 1, v);
		glUseProgram(currentProg);
	}

	RETURN_OK()
}

///
/// \brief To set 2D uniform value.
/// To set 2D uniform value
///
bool Compositor::setUniformValue2uiv(int passID, char* uniName, GLuint* v)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);
		GLint loc = glGetUniformLocation(p->second.shaderProgram, uniName);
		glUniform2uiv(loc, 2, v);
		glUseProgram(currentProg);
	}

	RETURN_OK()
}

///
/// \brief To set 3D uniform value.
/// To set 3D uniform value
///
bool Compositor::setUniformValue3uiv(int passID, char* uniName, GLuint *v)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);
		GLint loc = glGetUniformLocation(p->second.shaderProgram, uniName);
		glUniform3uiv(loc, 3, v);
		glUseProgram(currentProg);
	}

	RETURN_OK()
}

///
/// \brief To set 4D uniform value.
/// To set 4D uniform value
///
bool Compositor::setUniformValue4uiv(int passID, char* uniName, GLuint* v)
{
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);
		GLint loc = glGetUniformLocation(p->second.shaderProgram, uniName);
		glUniform4uiv(loc,4, v);
		glUseProgram(currentProg);
	}

	RETURN_OK()
}

///
/// \brief To set textures as input to the pass.
/// To set textures as input to the pass. It is pair : texChannel - texID. texChannel is to be used for glActiveTexture(GL_TEXTURE0 - GL_TEXTURE31) before the rendering, 
///	and textID is to be used glBindTexture(GL_TEXTURE_2D, texID).
/// We do not check whether the texture is valid. Should it be implemented here, or it is responsibility of main program ?
///
bool Compositor::setUniformTexture(int passID, char* texUniform, GLuint texID)
{
	//todo : return false if the program is wrong
	//todo : return false if the uniform can't be found
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		p->second.texInputs[texUniform] = texID;

		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);
		std::map<char*, GLuint>::iterator t = p->second.texInputs.begin();
		for (int i = 0; i < p->second.texInputs.size(); i++)
		{
			GLint texLoc = glGetUniformLocation(p->second.shaderProgram, t->first);
			glUniform1i(texLoc, i);
			++t;
		}
		glUseProgram(currentProg);
	}

	RETURN_OK()
}

///
/// \brief To remove texture uniform. This function might not be needed.
/// To remove texture uniform. This function might not be needed.
///
bool Compositor::deleteUniformTexture(int passID, char* texUniform)
{
	//todo : return false if the program is wrong
	//todo : return false if the uniform can't be found
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		std::map<char*, GLuint>::iterator p2 = p->second.texInputs.find(texUniform);
		if (p2 == p->second.texInputs.end()) RETURN_ERR(Compositor::TEXTURE_UNIFORM_NOT_FOUND)
		p->second.texInputs.erase(p2);

		int currentProg;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProg);
		glUseProgram(p->second.shaderProgram);

		//reset the texture binding with the uniform
		std::map<char*, GLuint>::iterator t = p->second.texInputs.begin();
		for (int i = 0; i < p->second.texInputs.size(); i++)
		{
			GLint texLoc = glGetUniformLocation(p->second.shaderProgram, t->first);
			glUniform1i(texLoc, i);
			++t;
		}
		glUseProgram(currentProg);

	}

	RETURN_OK()
}

///
/// \brief To set texture as the render target in which channel in MRT.
/// To set texture as the render target in which channel in MRT.
///
bool Compositor::setOutputTexture(int passID, int texChannel, GLuint texID)
{
	if (texChannel > 15) return false; //openGL limitation
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{

		p->second.texOutputs[texChannel] = texID;
		GLint drawFboId;
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFboId);

		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, p->second.fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + texChannel, GL_TEXTURE_2D, texID, 0);

		if (p->second.texOutputsChannels != nullptr) delete p->second.texOutputsChannels;

		p->second.texOutputsChannels = new GLenum[p->second.texOutputs.size()];
		std::map<int, GLuint>::iterator t;
		int idx = 0;
		for (t = p->second.texOutputs.begin(); t != p->second.texOutputs.end(); ++t)
		{
			p->second.texOutputsChannels[idx] = GL_COLOR_ATTACHMENT0 + t->first;
			idx++;
		}

		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFboId);
	}

	RETURN_OK()
}

///
/// \brief To delete a specific texture MRT. Might not be needed.
/// To delete a specific texture MRT. Might not be needed.
///
bool Compositor::deleteOutputTexture(int passID, int texChannel)
{
	if (texChannel > 15) return false; //openGL limitation
	std::map<int, pass>::iterator p = m_passes.find(passID);
	if (p == m_passes.end()) RETURN_ERR(Compositor::PASS_NOT_FOUND)
	else
	{
		std::map<int, GLuint>::iterator p2 = p->second.texOutputs.find(texChannel);

		if (p2 == p->second.texOutputs.end()) RETURN_ERR(Compositor::TEXTURE_OUTPUT_NOT_FOUND)

		p->second.texOutputs.erase(p2);

		GLint drawFboId;
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFboId);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, p->second.fbo);
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + texChannel, GL_TEXTURE_2D, 0, 0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFboId);
	}

	RETURN_OK()
}

///
/// \brief Initialize Vertex Shader.
/// To initialize Vertex Shader for composition operation. As it is just a simple full-screen quad drawing, it can be used for all other passes.
///
void Compositor::initializeVertexShader()
{
	static const char* vertex_shader_text =
		"#version 330 compatibility\n"
		"in vec3 vPos;\n"
		"out vec2 UV;\n"
		"void main()\n"
		"{\n"
		"mat4 MVP = mat4(vec4(2.0, 0.0, 0.0, 0.0),\n"
		"                vec4(0.0, 2.0, 0.0, 0.0),\n"
		"                vec4(0.0, 0.0, -1.0, 0.0),\n"
		"                vec4(-1.0, -1.0, 0.0, 1.0));\n"
		"    gl_Position = MVP * vec4(vPos, 1.0);\n"
		"    UV.xy = vPos.xy;"
		"}\n";

	m_shaderVertex = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(m_shaderVertex, 1, &vertex_shader_text, 0);
	glCompileShader(m_shaderVertex);
}

///
/// \brief To create buffer for storing quad for composition.
/// To create buffer for storing quad for composition.
///
void Compositor::initializeBufferObject()
{
	const GLfloat vertices[] = {
		0.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 0.0f
	};

	GLint bufferVertexArray, bufferArrayBuffer;
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &bufferVertexArray);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &bufferArrayBuffer);

	/* Allocate and assign a Vertex Array Object to our handle */
	glGenVertexArrays(1, &m_vertexArray);
	/* Bind our Vertex Array Object as the current used object */
	glBindVertexArray(m_vertexArray);

	// Generate 1 buffer, put the resulting identifier in vertexbuffer
	glGenBuffers(1, &m_vertexBuffer);
	// The following commands will talk about our 'vertexbuffer' buffer
	glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
	// Give our vertices to OpenGL.
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glBindVertexArray(bufferVertexArray);
	glBindBuffer(GL_ARRAY_BUFFER, bufferArrayBuffer);
}

///
/// \brief Save OpenGL states before rendering.
/// To save OpenGL states before starting rendering so that states/settings from main program does not pollute in.
///
void Compositor::pushState()
{
	GLint temp; GLboolean tempB;

	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &temp);	m_state.fbo = temp;
	glGetIntegerv(GL_VIEWPORT, m_state.viewport);
	glGetFloatv(GL_COLOR_CLEAR_VALUE, m_state.clearColor);
	glGetBooleanv(GL_DEPTH_WRITEMASK, &m_state.depthMask);

	glGetIntegerv(GL_CURRENT_PROGRAM, &temp);	m_state.shaderProgram = temp;
	glGetIntegerv(GL_ACTIVE_TEXTURE, &temp);	m_state.tex_active = temp;

	for (int i = 0; i < 32; i++)
	{
		glActiveTexture(GL_TEXTURE0 + i);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &temp);	m_state.tex_binds.push_back(temp);
	}

	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &m_state.bufferVertexArray);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &m_state.bufferArrayBuffer);

}

///
/// \brief Restore OpenGL states after rendering.
/// To restore OpenGL states after rendering so that states/settings from the compositor does not pollute out.
///
void Compositor::popState()
{
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_state.fbo);
	glViewport(m_state.viewport[0], m_state.viewport[1], m_state.viewport[2], m_state.viewport[3]);
	glClearColor(m_state.clearColor[0], m_state.clearColor[1], m_state.clearColor[2], m_state.clearColor[3]);
	glDepthFunc(m_state.depthMask);

	for (int i = 0; i < m_state.tex_binds.size(); i++)
	{
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, m_state.tex_binds[i]);
	}
	m_state.tex_binds.clear();

	glActiveTexture(m_state.tex_active);							
	glUseProgram(m_state.shaderProgram);							

	glBindVertexArray(m_state.bufferVertexArray);					
	glBindBuffer(GL_ARRAY_BUFFER, m_state.bufferArrayBuffer);		
}

///
/// \brief Render the specified pass.
/// Render the specified pass.
///
void Compositor::renderPassInternal(std::map<int, pass>::iterator p)
{
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, p->second.fbo);

	std::map<char*, GLuint>::iterator t = p->second.texInputs.begin();
	for (int i = 0; i < p->second.texInputs.size(); i++)
	{
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, t->second);
		++t;
	}

	glDrawBuffers(p->second.texOutputs.size(), p->second.texOutputsChannels);
	glViewport(0, 0, m_width, m_height);
	glClearColor(0.0, 0.0, 1.0, 1.0);
	glUseProgram(p->second.shaderProgram);
	glBindVertexArray(m_vertexArray);
	glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
	glClear(GL_COLOR_BUFFER_BIT);
	glDepthMask(GL_FALSE);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}
///
/// \brief To verify whether a set of passes are usable.
/// To verify whether a set of passes are usable.
///
bool Compositor::verifyPipeline(std::vector<int> pipeline)
{
	for (int i = 0; i < pipeline.size(); i++)
	{
		std::map<int, pass>::iterator p = m_passes.find(pipeline[i]);
		if (p == m_passes.end()) return false;
		else if (p->second.initialized == false) return false;
		else if (p->second.texOutputs.size() == 0) return false;
	}

	return true;
}