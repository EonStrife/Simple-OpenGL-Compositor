# Simple OpenGL Compositor

A very simple class for performing post-processing/composition using OpenGL Fragment shader. 

## Getting Started

Copy and include the Compositor.cpp and Compositor.h files into your project and you can start using it right away.

### Prerequisites

GPU and Graphics Drivers supporting minimum OpenGL 3.0. For development in Microsoft Windows environment, you might need [GLEW](http://glew.sourceforge.net/) or other similar libraries in order to access some of the OpenGL functionalities used in the class.

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

## Versioning

We use [SemVer](http://semver.org/) for versioning. For the versions available, see the [tags on this repository](https://github.com/your/project/tags). 

## Author

* **Budianto Tandianus** - *Initial work* - [EonStrife](https://github.com/EonStrife/)

## License

This project is licensed under the MIT License - see the [LICENSE.txt](LICENSE.txt) file for details


