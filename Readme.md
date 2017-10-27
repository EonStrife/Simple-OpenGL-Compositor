# Simple OpenGL Compositor

A very simple class for performing post-processing/composition using OpenGL Fragment shader. Right now, the following OpenGL shader features are not supported :
- Uniform Buffer Object
- Separate Shader Object
- Shaders Subroutine

## Getting Started

Copy and include the [Compositor.cpp](Compositor.cpp) and [Compositor.h](Compositor.h) files into your project and you can start using it right away.

### Prerequisites

GPU and its driver which support minimum OpenGL 3.0. For development in Microsoft Windows environment, you might need [GLEW](http://glew.sourceforge.net/) or other similar libraries in order to access some of OpenGL functionalities used in the class.

### How to Use

#### Basic Usage

```
Compositor *compositor;
int window_width, window_height;
int pass;
GLuint texInputs[2];
GLuint texOutputs[2];
void initializeCompositor()
{
	compositor = new Compositor();
	compositor->setResolution(window_width, window_height);

	pass  = compositor->createNewPass();
	compositor->loadShader(pass, "effect.frag")	
	compositor->setUniformTexture(pass, "tex1", texInputs[0]);
	compositor->setUniformTexture(pass, "tex2", texInputs[1]);
	compositor->setOutputTexture(pass, 0, texOutpus[0]);
	compositor->setOutputTexture(pass, 1, texOutpus[1]);
	compositor->setUniformValue3f(pass, "scale", 0.5);
}

void drawCompositor()
{
	compositor->renderPass(pass);
}
```

- ```pass``` is ID for the current rendering pass. 
- ```texInputs``` and ```texOutputs``` are the OpenGL texture names. The main application is responsible for generating textures to be used as inputs and outputs of rendering passes.
- ```setResolution``` is self-explanatory.
- ```loadShader(...)``` is for loading fragment shader for composition operation. The second parameter is the filename of a fragment shader.
- ```setUniformTexture(...)``` is for setting textures as uniform input into the fragment shader. The second parameter is the uniform name in fragment shader, and the third parameter is the name of OpenGL texture to be used as a input texture uniform.
- ```setOutputTexture(...)``` is for setting textures for rendering target. The second parameter is the color attachment index (corresponds to location number in output specification in fragment shader, e.g. ```layout( location = 0 ) out vec4 oColor;```), and the third parameter is the name of OpenGL texture to be used as rendering target. This class supports Multiple Render Target. In the example, we render to two textures.
- ```setUniformValue*(...)``` is for setting uniform value. We use same variable qualifier suffixes as the one in ```glUniform*```.
- ```renderPass(...)``` is for rendering the pass.

#### Fragment Shader
As composition using OpenGL generally renders a simple quad to a whole screen, the vertex shader is hardcoded inside the ```Compositor``` class. It is a simple vertex shader which outputs 2D UV coordinate to be used in sampling textures in fragment shader. Thus, for the fragment shaders you want to use, you need to define the 2D ```UV``` varying input in your fragment shaders:

```
in vec2 UV;	

uniform sampler2D tex1;
layout( location = 0 ) out vec4 oColor;
void main()
{
	oColor = vec4(texture(tex1, UV).xyz, 1.0);
}
```
The UV coordinate for sampling has the range of 0.0 to 1.0 in both dimensions.

#### Multiple Passes

The class also support sequential multiple pass rendering (we call it as pipeline). In the following example, we are going to do a sequence of rendering passes comprising three passes. We assume each rendering pass depends on its previous rendering pass (i.e. output texture from a rendering pass is used as an input texture uniform for the next rendering pass).
```
int pass1, pass2, pass3;
int pipeline;

void initializeCompositor()
{
	pass1 = compositor->createNewPass();
	pass2 = compositor->createNewPass();
	pass3 = compositor->createNewPass();
	// Set uniforms and render targets for the three  passes.
}

void initializePipeline()
{
	std::vector<int> pipelineLayout;
	pipelineLayout.push_back(pass1);
	pipelineLayout.push_back(pass2);
	pipelineLayout.push_back(pass3);
	pipeline = compositor->createSequentialPipeline();
	compositor->setPipeline(pipeline, pipelineLayout);
}

void drawCompositor()
{
//	Equivalent to :
//	compositor->renderPass(pass1);
//	compositor->renderPass(pass2);
//	compositor->renderPass(pass3);
//
	compositor->renderPipeline(pipeline);
}
```

Similar to pass rendering, we also have ID for each pipeline (it is created by using ```createSequentialPipeline()```. The three passes are stored in ```std::vector``` and they are passed to the ```Compositor``` class using the ```setPipeline(...)``` function. To start the sequence of rendering, we call the ```renderPipeline(...)``` function.

#### Error Handling

Most functions will return boolean values denoting the process is succesful or not. If something is wrong, the functions will return ```false```. To check what is the error, we call the ```getLastError()``` function.

```
	Compositor::error e = Compositor::NONE;
	if (!compositor->loadShader(pass, "effect.frag"))
		e = compositor->getLastError();
```

The Compositor also provides a functionality to return error string of OpenGL shader compilation using the function ```getLastShaderError()```. 

```
	if (!compositor->loadShader(pass, "effect.frag"));
	{
		e = compositor->getLastError();
		std::cout << compositor->getLastShaderError() << std::endl;
	}

```

## Author

* **Budianto Tandianus** - *Initial work* - [EonStrife](https://github.com/EonStrife/)

## License

This project is licensed under the MIT License - see the [License.txt](License.txt) file for details


