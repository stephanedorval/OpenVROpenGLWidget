/// \file OpenVROpenGLWidget.h
/// \brief Declare a C++ class for OpenVR display in a Qt OpenGL widget.

#ifndef __OPENVROPENGLWIDGET_H__
#define __OPENVROPENGLWIDGET_H__

//  OpenVR SDK includes
#include <openvr.h>

// Qt OpenGL includes
#include <QOpenGLFunctions_4_5_core>
#include <QOpenGLWidget>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#ifdef _DEBUG
#include <QOpenGLDebugMessage>
#include <QOpenGLDebugLogger>
#endif

// Qt includes
#include <QMatrix4x4>
#include <QVector3D>


/// \class	COpenVROpenGLWidget
/// \brief	Define a widget which renders a scene in a virtual reality headset and in the widget.
///			It is mostly inspired by samples from OpenVR SDK.
class COpenVROpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions_4_5_Core
{
	Q_OBJECT


	/// \class		CEyesInfos
	/// \brief		A usefull class to deal with display, framebuffers and transformations of eyes in the head mounted display.
	/// \details	The contructor and the destructor respectively generate and destroy the frame buffers used to display the
	///				scene in the headset. 
	///				The projection matrix and the view matrix are here only for storage.
	///				To render in HMD, first call \c SetSurface(), render your scene and then, call \c UnsetSurface(). This will
	///				populate the frame buffers to commit to the vr system.
	///				It is developer's responsaibility to know witch instance of \c CEyeInfos refers to witch eye (right or left).
	class CEyeInfos : protected QOpenGLFunctions_4_5_Core
	{
		/// The projection matrix of the eye according to the MVP transform model.
		QMatrix4x4 m_projection;

		/// The view matrix of the eye according to the MVP transform model.
		QMatrix4x4 m_view;

		/// The frame buffer objet to render in.
		QOpenGLFramebufferObject* m_frameBuffer;

		/// The frame buffer objet used to grab a texture.
		QOpenGLFramebufferObject* m_resolveBuffer;

		/// The size in pixels of the texture of the eye.
		QSize m_size;

	public:

		/// \brief	Constructor: format and create frame buffers for rendering.
		///	\param	i_eyeSize	The size of the output texture to submit to the vr system.
		CEyeInfos(const QSize& i_eyeSize);

		/// \brief	Descrutor: delete buffers properly.
		~CEyeInfos();

		/// \brief	Initialize and prepare the scene rendering. Set and bind the buffers.
		/// \note	Must be call just \e before scene rendering.
		void SetSurface();

		/// \brief	Finish the rendering session by creating a texture.
		///	\note	Must be call just \e after scene rendering.
		void UnsetSurface();

		/// \brief	Accessor to the texture generated.
		/// \return	The ID of the texture of the frame.
		GLuint Texture();

		/// \brief	Update the projection and the view matrix for this eye.
		/// \param	i_view			The new view matrix for the eye display.
		/// \param	i_projection	The new projection matrix for the eye display.
		void SetTransformMatrix(const QMatrix4x4& i_view, const QMatrix4x4& i_projection);

		/// \brief	Accessor to the projection matrix.
		///	\return	A 4 x 4 matrix with de values of the projection.
		const QMatrix4x4& GetProjectionMatrix();

		/// \brief	Accessor to the view matrix.
		///	\return	A 4 x 4 matrix with de values of the view.
		const QMatrix4x4& GetViewMatrix();

		/// \brief	Determine if the frame buffers were created.
		/// \return \c true if the frame buffers were create correctly, \c false otherwise.
		bool IsValid();
	};


	/// \class		CRenderModel
	/// \brief		A useful class to build and display a 3D objet of a controller according to the vr system version.
	///	\details	The constructor build a 3D objet of the version of the controller given by its name. For each vr 
	///				system, each controller device is deferent and OpenVR SDK has a 3D model of the most popular ones.
	///				This class let the developer to create and display a controller easily, just give the name of the
	///				controller you want to use.
	///				Then, just call \c Draw() to display it.
	class CRenderModel : protected QOpenGLFunctions_4_5_Core
	{
		///	The program shader to display the controller.
		QOpenGLShaderProgram* m_program;

		/// The vertex buffer object ID.
		GLuint m_glVertBuffer;

		/// The element buffer object ID.
		GLuint m_glIndexBuffer;

		/// The vertex array buffer ID.
		GLuint m_glVertArray;

		/// The controller's texture ID.
		GLuint m_glTexture;

		/// The total number of vertices of the 3D model.
		GLsizei m_unVertexCount;

		/// The controller's name.
		QString m_sModelName;

		/// \brief	Constructor: build the default shader program.
		///	\param	i_sRenderModelName	The name of the controller to build.
		/// \todo	Let the developper choose his own program.
		CRenderModel(const QString & i_sRenderModelName);

		/// \brief	Build the 3D model of the controller.
		/// \param	o_vrModel			The built 3D model to render.
		///	\param	o_vrDiffuseTexture	The texture of the built 3D model.
		bool InitModel(const vr::RenderModel_t & o_vrModel, const vr::RenderModel_TextureMap_t & o_vrDiffuseTexture);

		/// \brief	Clean OpenGL buffers properly.
		void Cleanup();

	public:

		/// \brief	Destructor: clean OpenGL buffers properly.
		~CRenderModel();

		/// \brief	Display the controller in the scene accordinf to the MVP matrix given as a parameter.
		/// \param	i_mvpMatrix	The transform matrix of the controller according to the MVP transform model.
		///	\note	MVP matrix can by retrieve with SDK methods.
		void Draw(const QMatrix4x4& i_mvpMatrix);

		/// \brief	Accessor to the name of this instance of device.
		/// \return The string containing the name of the device.
		const QString& GetName() const { return m_sModelName; }

		/// \brief	Static method to load a 3D model according to its name given as a parameter.
		/// \param	i_modelName		The name of the device to load and build.
		/// \param	o_errorMeesage	And optional pointer to a string to get the error.
		/// \return The instance of the controller named \c i_modelName.
		static CRenderModel* LoadModel(const QString& i_modelName, QString* o_errorMeesage = nullptr);
	};


	/// \struct	SControllerInfos
	/// \brief	Storage structure to store controllers informations like transforms, 3D objets etc...
	struct SControllerInfos
	{
		/// The controller model transform matrix.
		QMatrix4x4 m_rmat4Pose;

		/// The instance of 3D model to display.
		CRenderModel *m_pRenderModel = nullptr;
		
		/// Determine if the controller must be displayed (not dplayed in cas of load error).
		bool m_bShowController = false;
	};


public:

	/// \enum	Eye
	/// \brief	Define hand and left eye's IDs.
	enum Eye {
		Left,
		Right
	};

	/// \brief	Construct the OpenVR OpenGL widget
	/// \param	i_parent	The parent widget
	explicit COpenVROpenGLWidget(QWidget *i_parent = nullptr);

	/// \brief	Destructor
	virtual ~COpenVROpenGLWidget();

	/// \brief	Method to initialize a scene rendering.
	/// \note	Must be implemented. Called in initializeGL() method.
	virtual void InitializeRendering() = 0;

	/// \brief	Method to update the scene (update animations for example)
	/// \note	Must be implemented. Called in paintGL() method.
	virtual void UpdateRendering() = 0;

	/// \brief	Method to render the scene.
	/// \param	eye			Gives to which eye to render (left or right)
	/// \param	view		The model view matrix.
	/// \param	projection	The projection matrix.
	/// \note	Must be implemented. Called in paintGL() method.
	virtual void Render(Eye eye, const QMatrix4x4& view, const QMatrix4x4& projection) = 0;

	/// \brief		Method to let the developer to initialize the controller's intput managment.
	/// \details	If the application needs to use controller's buttons, triggers, joysick and others use this
	///				this method to set de manifest actions file et defined all the actions handler according to 
	///				the OpenVR SDK vr::Input() class.
	/// \note		Must be implemented. Called in InitializeControllers() method.
	virtual void InitializeInputs() = 0;

	/// \brief		Method to process the inputs defined in \c InitializeInputs().
	///	\details	Get the intputs state and process the event.
	virtual void UpdateInputs() = 0;

	/// \brief	Translate eyes positions by the vector (i_deltaX, i_deltaY, i_deltaZ).
	/// \param	i_deltaX	Translation value on X axis.
	/// \param	i_deltaY	Translation value on Y axis.
	/// \param	i_deltaZ	Translation value on Z axis.
	void TranslateEyes(float i_deltaX, float i_deltaY, float i_deltaZ);

	/// \brief Reset eyes position to (0.0, 0.0, 0.0).
	void ResetEyesPositions();

	/// \return The eyes translation vector.
	QVector3D GetTranslations();

	/// \brief	Rotate eyes by the angles (i_yaw, i_pitch, i_roll).
	/// \param	i_yaw	Rotation angle value on Y axis.
	/// \param	i_pitch	Rotation angle value on X axis.
	/// \param	i_roll	Rotation angle value on Z axis.
	void RotateEyes(float i_yaw, float i_pitch, float i_roll);

	/// \brief Reset eyes rotations to (0.0, 0.0, 0.0).
	void ResetEyesRotations();

	/// \return The eyes Euler's angles in vector (yaw, pitch, roll).
	QVector3D GetRotations();

	/// \brief	Accessor to the controller transform matrix associated to a hand given as a parameter.
	///	\param	i_hand	The considered hand ID.
	/// \return	A 4x4 matrix with the value of the transform matrix.
	const QMatrix4x4& GetControllerPose(int i_hand);

	/// \brief	Accessor to the camere transforme matrix.
	/// \return	A 4x4 matrix with the value of the camera transform matrix.
	QMatrix4x4 GetCameraMatrix();

#ifdef _DEBUG

protected slots:

	/// \brief	Slot to pop-up the OpenGL error.
	void debugMessage(QOpenGLDebugMessage i_message);

#endif

protected:

	// From QOpenGLWidget...

	void initializeGL();
	void paintGL();
	void resizeGL(int w, int h);

private:

	/// The virtual reality system.
	vr::IVRSystem* m_vrSystem;

	/// The position information of all devices of the vr system.
	vr::TrackedDevicePose_t m_trackedDevicePose[vr::k_unMaxTrackedDeviceCount];

	/// The transformation matrix of all devices of the vr system.
	QMatrix4x4 m_matrixDevicePose[vr::k_unMaxTrackedDeviceCount];

	/// The transformation matrix of the head mounted display.
	QMatrix4x4 m_hmdPose;

#ifdef _DEBUG
	/// The OpenGL logger.
	QOpenGLDebugLogger *m_logger;
#endif

	///	The eyes informations: transformations, OpenGL buffers, display method...
	CEyeInfos* m_eyeInfos[2];

	/// The controllers informations: transformations, 3D models; display method...
	SControllerInfos m_controllers[2];

	/// Eyes translation vector
	QVector3D m_cameraTranslation;

	/// Eyes rotation angles: yaw, pitch, roll
	QVector3D m_cameraRotations;

	/// Initialize the VR system
	bool InitializeVR();

	/// Initialize the left and right eyes informations.
	bool InitializeEyesRendering();

	/// Initialisez the left and right controllers models.
	bool InitializeControllers();

	///	Clean and destroy OpenGL objects.
	void Destroy();

	/// Switch off the vr system.
	void ShutDownVR();

	/// \brief	Render the scene for the eye given as parameter.
	///	\param	i_eye	The considered eye ID.
	void renderEye(Eye i_eye);

	/// Update the positions and the transformations of the eyes, the controllers, etc...
	void UpdatePositions();

	/// \brief	Convert a matrix from a \c HmdMatrix34_t format to a \c QMatrix4x4 matrix format.
	/// \param	i_mat	The matrix to convert.
	/// \return The converted \c QMatrix4x4 matrix.
	QMatrix4x4 vrMatrixToQt(const vr::HmdMatrix34_t &i_mat);

	/// \brief	Convert a matrix from a \c HmdMatrix44_t format to a \c QMatrix4x4 matrix format.
	/// \param	i_mat	The matrix to convert.
	/// \return The converted \c QMatrix4x4 matrix.
	QMatrix4x4 vrMatrixToQt(const vr::HmdMatrix44_t &mat);

	/// \brief	Retrieve a string value from a device property.
	/// \param	i_device	The index of the device we want to get the property.
	///	\param	i_prop		The property ID to retrieve.
	///	\param	o_error		The returned error in case of failure.
	/// \return The property value as a string.  
	QString getTrackedDeviceString(vr::TrackedDeviceIndex_t i_device,	vr::TrackedDeviceProperty i_prop,	vr::TrackedPropertyError *o_error = nullptr);
};

#endif // __OPENVROPENGLWIDGET_H__
