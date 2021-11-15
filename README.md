# OpenVROpenGLWidget

## Description
A Qt OpenGL widget for OpenVR.

## Installation
Copy the COpenVROpenGLWidget C++ class source files to your projet.
* OpenVROpenGLWidget.h
* OpenVROpenGLWidget.cpp

## Dependencies
The COpenVROpenGLWidget class depends on:
* Qt framework >= 5.5
* OpenVR SDK >= 1.16.8 (not tested with previous versions)
   
## Usage
Define your own widget which inherits from OpenVROpenGLWidget. Then, there are 5 methods
to implement:
* **InitializeRendering()** which is called in the initializeGL() method of QOpenGLWidget.
Here you should initialize your own scene.
* **UpdateRendering(...)** which is called int the paintGL() method of QOpenGLWidget before
scene rendering to each eye. Here you should update the animated parts of the scene relative
to both eyes.
* **Render(...)** which is called in the paintGL() method of QOpenGLWidget.
Here you should render your scene. It is called for each eye.
* **InitializeInputs()** which is called in the InitializeControllers() method.
Here you should set up the controllers actions according to the vr::Input interface specifications.
* **InitializeInputs()** which is called in the paintGL() method of QOpenGLWidget.
Here you should update your scene according to the actions handles already defined.

## Licence
This OpenVROpenGLWidget C++ class is licensed with the GNU GPLv3 licence.
See LICENCE file.