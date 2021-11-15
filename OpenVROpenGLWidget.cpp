/// \file COpenVROpenGLWidget.cpp
/// \brief Implement the C++ class for OpenVR VR display in a Qt OpenGL widget declared in CCOpenVROpenGLWidget.h.

#include "OpenVROpenGLWidget.h"

#include <thread>

#include <QMessageBox>
#include <QDebug>

#define INITIAL_ROTATION	QVector3D(0.0f, 180.0f, 0.0f)
#define INITIAL_TRANSLATION QVector3D(0.0f, 0.0f, 0.0f)
#define	DEFAULT_WIN_SIZE	QSize(1024,720)

#define NEAR_CLIP	0.1f
#define FAR_CLIP	10000.0f

COpenVROpenGLWidget::COpenVROpenGLWidget(QWidget *parent) : 
	QOpenGLWidget(parent),
	m_vrSystem(nullptr),
	m_cameraTranslation(INITIAL_TRANSLATION),
	m_cameraRotations(INITIAL_ROTATION)
{
	m_eyeInfos[Left] = m_eyeInfos[Right] = nullptr;
}

COpenVROpenGLWidget::~COpenVROpenGLWidget()
{
	ShutDownVR();
}

void COpenVROpenGLWidget::Destroy()
{
	for (int eye = 0; eye < 2; eye++)
	{
		delete m_eyeInfos[eye];
	}

	for (int hand = 0; hand < 2; hand++)
	{
		delete m_controllers[hand].m_pRenderModel;
	}

#ifdef _DEBUG
	delete m_logger;
#endif
}

void COpenVROpenGLWidget::ShutDownVR()
{
	makeCurrent();

	Destroy();

	if (m_vrSystem)
	{
		vr::VR_Shutdown();
		m_vrSystem = nullptr;
	}

	doneCurrent();
}

#ifdef _DEBUG
void COpenVROpenGLWidget::debugMessage(QOpenGLDebugMessage message)
{
	qDebug() << message;
}
#endif

void COpenVROpenGLWidget::initializeGL()
{
	initializeOpenGLFunctions();

#ifdef _DEBUG
	m_logger = new QOpenGLDebugLogger(this);

	connect(m_logger, SIGNAL(messageLogged(QOpenGLDebugMessage)), this, SLOT(debugMessage(QOpenGLDebugMessage)), Qt::DirectConnection);

	if (m_logger->initialize())
	{
		m_logger->startLogging(QOpenGLDebugLogger::SynchronousLogging);
		m_logger->enableMessages();
	}
#endif

	glEnable(GL_DEPTH_TEST);

	if (!InitializeVR())
		return;
	
	// create eyes
	if (!InitializeEyesRendering()) // Pop up error message
		return;

	// create controllers
	InitializeControllers(); // Pop up error message

	// init scene
	InitializeRendering();
}

bool COpenVROpenGLWidget::InitializeVR()
{
	// Check whether there’s an HMD connected and a runtime installed 
	QString errMessage;
	if (!vr::VR_IsRuntimeInstalled())
	{
		errMessage = QString("No VR runtime installed.");
		qDebug() << errMessage;
		QMessageBox::critical(this, windowTitle(), errMessage);
		return false;
	}
	
	if (!vr::VR_IsHmdPresent())
	{
		errMessage = QString("Any headset found.");
		qDebug() << errMessage;
		QMessageBox::critical(this, windowTitle(), errMessage);
		return false;
	}

	// Initialize device
	vr::EVRInitError initErr = vr::VRInitError_Unknown;
	m_vrSystem = vr::VR_Init(&initErr, vr::VRApplication_Scene);
	if (initErr != vr::VRInitError_None)
	{
		m_vrSystem = nullptr;
		errMessage = QString("Unable to init VR runtime: %1").arg(vr::VR_GetVRInitErrorAsEnglishDescription(initErr));
		qDebug() << errMessage;
		QMessageBox::critical(this, windowTitle(), errMessage);
		return false;
	}

	// Initialize compositor
	if (!vr::VRCompositor())
	{
		errMessage = "Compositor initialization failed. See log file for details";
		qCritical() << errMessage;
		QMessageBox::critical(this, windowTitle(), errMessage);
		vr::VR_Shutdown();
		m_vrSystem = nullptr;
		return false;
	}

	return true;
}


bool COpenVROpenGLWidget::InitializeEyesRendering()
{
	// get eye size
	uint32_t eyeWidth, eyeHeight;
	m_vrSystem->GetRecommendedRenderTargetSize(&eyeWidth, &eyeHeight);
	QSize eyeSize(static_cast<int>(eyeWidth), static_cast<int>(eyeHeight));

	// create eyes
	bool noErr = true;
	for (int eye = 0; eye < 2; eye++)
	{
		m_eyeInfos[eye] = new CEyeInfos(eyeSize);
		noErr &= m_eyeInfos[eye]->IsValid();
	}

	return noErr;
}

bool COpenVROpenGLWidget::InitializeControllers()
{
	// Get devices positions and information
	vr::VRCompositor()->WaitGetPoses(m_trackedDevicePose, vr::k_unMaxTrackedDeviceCount, NULL, 0);

	QString controllerName;
	for (unsigned int i = 0; i < vr::k_unMaxTrackedDeviceCount; i++)
	{
		if (m_vrSystem->GetTrackedDeviceClass(i) != vr::TrackedDeviceClass_Controller)
			continue;

		controllerName = getTrackedDeviceString(i, vr::Prop_RenderModelName_String);
		
		int handIndex = (vr::ETrackedControllerRole::TrackedControllerRole_LeftHand == m_vrSystem->GetControllerRoleForTrackedDeviceIndex(i)) ? Left : Right;
		m_controllers[handIndex].m_pRenderModel = COpenVROpenGLWidget::CRenderModel::LoadModel(controllerName);
		m_controllers[handIndex].m_bShowController = true;
	}

	InitializeInputs();

	return true;
}

void COpenVROpenGLWidget::paintGL()
{
	if (m_vrSystem)
	{
		// Update eyes and devices matrix transform
		UpdatePositions();

		// Updates acording to controllers actions
		UpdateInputs();

		glClearColor(0.15f, 0.15f, 0.18f, 1.0f);

		UpdateRendering();

		// Render for eyes
		for (int eye = 0; eye < 2; eye++)
		{
			m_eyeInfos[eye]->SetSurface();
			renderEye(static_cast<Eye>(eye));
			m_eyeInfos[eye]->UnsetSurface();
		}
	}

	// Render mirror view in window
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glViewport(0, 0, width(), height());
	glDisable(GL_MULTISAMPLE);
	renderEye(Right);

	if (m_vrSystem)
	{
		for (int eye = 0; eye < 2; eye++)
		{
			vr::Texture_t composite = { (void*)m_eyeInfos[eye]->Texture(), vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
			vr::VRCompositor()->Submit(static_cast<vr::EVREye>(eye), &composite);
		}
	}

	update();
}

void COpenVROpenGLWidget::renderEye(Eye i_eye)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

	const QMatrix4x4& projection = m_eyeInfos[i_eye]->GetProjectionMatrix();
	const QMatrix4x4 view = m_eyeInfos[i_eye]->GetViewMatrix() * m_hmdPose;
	
	// Render controller
	QMatrix4x4 matVP = projection * view;
	for (int hand = 0; hand < 2; hand++)
	{
		if (!m_controllers[hand].m_bShowController)
			continue;
		m_controllers[hand].m_pRenderModel->Draw(matVP * m_controllers[hand].m_rmat4Pose);
	}

	// Render scene
	Render( i_eye, view * GetCameraMatrix(), projection);
}

void COpenVROpenGLWidget::resizeGL(int, int)
{
	// do nothing
}

void COpenVROpenGLWidget::UpdatePositions()
{
	// Get eyes matrices
	for (int eye = 0; eye < 2; eye++)
	{
		m_eyeInfos[eye]->SetTransformMatrix(
			vrMatrixToQt(m_vrSystem->GetEyeToHeadTransform(static_cast<vr::EVREye>(eye))).inverted(),
			vrMatrixToQt(m_vrSystem->GetProjectionMatrix(static_cast<vr::EVREye>(eye), NEAR_CLIP, FAR_CLIP))
		);
	}

	// Get devices matrices
	vr::VRCompositor()->WaitGetPoses(m_trackedDevicePose, vr::k_unMaxTrackedDeviceCount, NULL, 0);

	for (unsigned int nDevice = 0; nDevice < vr::k_unMaxTrackedDeviceCount; nDevice++)
	{
		if (m_trackedDevicePose[nDevice].bPoseIsValid)
		{
			m_matrixDevicePose[nDevice] = vrMatrixToQt(m_trackedDevicePose[nDevice].mDeviceToAbsoluteTracking);
			
			switch (m_vrSystem->GetTrackedDeviceClass(nDevice))
			{

			case vr::TrackedDeviceClass_Controller:
			{
				int handIndex = (vr::ETrackedControllerRole::TrackedControllerRole_LeftHand == m_vrSystem->GetControllerRoleForTrackedDeviceIndex(nDevice)) ? Left : Right;
				m_controllers[handIndex].m_rmat4Pose = vrMatrixToQt(m_trackedDevicePose[nDevice].mDeviceToAbsoluteTracking);
			}
				break;

			case vr::TrackedDeviceClass_HMD:
				m_hmdPose = m_matrixDevicePose[vr::k_unTrackedDeviceIndex_Hmd].inverted();
				break;

			default:
				break;
			}
		}
	}
}

QMatrix4x4 COpenVROpenGLWidget::vrMatrixToQt(const vr::HmdMatrix34_t &mat)
{
	return QMatrix4x4(
		mat.m[0][0], mat.m[0][1], mat.m[0][2], mat.m[0][3],
		mat.m[1][0], mat.m[1][1], mat.m[1][2], mat.m[1][3],
		mat.m[2][0], mat.m[2][1], mat.m[2][2], mat.m[2][3],
		0.0, 0.0, 0.0, 1.0f
	);
}

QMatrix4x4 COpenVROpenGLWidget::vrMatrixToQt(const vr::HmdMatrix44_t &mat)
{
	return QMatrix4x4(
		mat.m[0][0], mat.m[0][1], mat.m[0][2], mat.m[0][3],
		mat.m[1][0], mat.m[1][1], mat.m[1][2], mat.m[1][3],
		mat.m[2][0], mat.m[2][1], mat.m[2][2], mat.m[2][3],
		mat.m[3][0], mat.m[3][1], mat.m[3][2], mat.m[3][3]
	);
}

QString COpenVROpenGLWidget::getTrackedDeviceString(vr::TrackedDeviceIndex_t i_device, vr::TrackedDeviceProperty i_prop, vr::TrackedPropertyError *o_error)
{
	uint32_t len = m_vrSystem->GetStringTrackedDeviceProperty(i_device, i_prop, NULL, 0, o_error);
	if (len == 0)
		return "";

	char *buf = new char[len];
	m_vrSystem->GetStringTrackedDeviceProperty(i_device, i_prop, buf, len, o_error);

	QString result = QString::fromLocal8Bit(buf);
	delete[] buf;

	return result;
}

void COpenVROpenGLWidget::TranslateEyes(float i_deltaX, float i_deltaY, float i_deltaZ)
{
	QMatrix4x4 rollPitchYaw;
	rollPitchYaw.rotate(QQuaternion::fromEulerAngles(m_cameraRotations.x(), m_cameraRotations.y(), m_cameraRotations.z()));
	QVector3D translation = rollPitchYaw * QVector3D(i_deltaX, i_deltaY, i_deltaZ);
	m_cameraTranslation += translation;
}

void COpenVROpenGLWidget::ResetEyesPositions()
{
	m_cameraTranslation = INITIAL_TRANSLATION;
}

QVector3D COpenVROpenGLWidget::GetTranslations()
{
	return m_cameraTranslation;
}

void COpenVROpenGLWidget::RotateEyes(float i_yaw, float i_pitch, float i_roll)
{
	m_cameraRotations += QVector3D(i_yaw, i_pitch, i_roll);
}

void COpenVROpenGLWidget::ResetEyesRotations()
{
	m_cameraRotations = INITIAL_ROTATION;
}

QVector3D COpenVROpenGLWidget::GetRotations()
{
	return m_cameraRotations;
}

const QMatrix4x4& COpenVROpenGLWidget::GetControllerPose(int i_hand)
{
	return m_controllers[i_hand].m_rmat4Pose;
}

QMatrix4x4 COpenVROpenGLWidget::GetCameraMatrix()
{
	QMatrix4x4 cameraTransform;
	cameraTransform.rotate(QQuaternion::fromEulerAngles(m_cameraRotations));
	cameraTransform.translate(m_cameraTranslation);
	return cameraTransform;
}









// ////////////////////////////////////////////////////////////////////////////////////////////////
//
//	EYE INFORMATIONS FOR RENDERING
//

COpenVROpenGLWidget::CEyeInfos::CEyeInfos(const QSize& i_eyeSize) :
	m_size(i_eyeSize),
	m_frameBuffer(nullptr),
	m_resolveBuffer(nullptr)
{
	initializeOpenGLFunctions();

	QOpenGLFramebufferObjectFormat buffFormat;
	buffFormat.setAttachment(QOpenGLFramebufferObject::Depth);
	buffFormat.setInternalTextureFormat(GL_RGBA8);
	buffFormat.setSamples(4);

	m_frameBuffer = new QOpenGLFramebufferObject(m_size.width(), m_size.height(), buffFormat);

	QOpenGLFramebufferObjectFormat resolveFormat;
	resolveFormat.setInternalTextureFormat(GL_RGBA8);
	resolveFormat.setSamples(0);

	m_resolveBuffer = new QOpenGLFramebufferObject(m_size.width(), m_size.height(), resolveFormat);
}

COpenVROpenGLWidget::CEyeInfos::~CEyeInfos()
{
	delete m_resolveBuffer;
	delete m_frameBuffer;
}

void COpenVROpenGLWidget::CEyeInfos::SetSurface()
{
	glViewport(0, 0, m_size.width(), m_size.height());

	glEnable(GL_MULTISAMPLE);
	m_frameBuffer->bind();
}

void COpenVROpenGLWidget::CEyeInfos::UnsetSurface()
{
	m_frameBuffer->release();

	QRect sourceAndTargetRect(0, 0, m_size.width(), m_size.height());
	QOpenGLFramebufferObject::blitFramebuffer(m_resolveBuffer, sourceAndTargetRect, m_frameBuffer, sourceAndTargetRect);
}

GLuint COpenVROpenGLWidget::CEyeInfos::Texture()
{
	return m_resolveBuffer->texture();
}

void COpenVROpenGLWidget::CEyeInfos::SetTransformMatrix(const QMatrix4x4& i_view, const QMatrix4x4& i_projection)
{
	m_view = i_view;
	m_projection = i_projection;
}

const QMatrix4x4&  COpenVROpenGLWidget::CEyeInfos::GetProjectionMatrix()
{
	return m_projection;
}

const QMatrix4x4&  COpenVROpenGLWidget::CEyeInfos::GetViewMatrix()
{
	return m_view;
}

bool COpenVROpenGLWidget::CEyeInfos::IsValid()
{
	return (m_frameBuffer != nullptr) && (m_resolveBuffer != nullptr) && m_frameBuffer->isValid() && m_resolveBuffer->isValid();
}








// ////////////////////////////////////////////////////////////////////////////////////////////////
//
//	EYE INFORMATIONS FOR RENDERING
//

#define RENDERMODEL_VERTEX_SHADER \
	"#version 450\n" \
	"uniform mat4 matrix;\n" \
	"layout(location = 0) in vec4 position;\n" \
	"layout(location = 1) in vec3 v3NormalIn;\n" \
	"layout(location = 2) in vec2 v2TexCoordsIn;\n" \
	"out vec2 v2TexCoord;\n" \
	"void main()\n" \
	"{\n" \
	"	v2TexCoord = v2TexCoordsIn;\n" \
	"	gl_Position = matrix * vec4(position.xyz, 1);\n" \
	"}\n"

#define RENDERMODEL_FRAGMENT_SHADER \
	"#version 450 core\n" \
	"uniform sampler2D diffuse;\n" \
	"in vec2 v2TexCoord;\n" \
	"layout(location = 0) out vec4 FragColor;\n" \
	"void main()\n" \
	"{\n" \
	"   FragColor = texture( diffuse, v2TexCoord);\n" \
	"}\n"

COpenVROpenGLWidget::CRenderModel::CRenderModel(const QString& i_sRenderModelName) :
	m_sModelName(i_sRenderModelName),
	m_glIndexBuffer(0),
	m_glVertArray(0),
	m_glVertBuffer(0),
	m_glTexture(0),
	m_program(new QOpenGLShaderProgram())
{
	initializeOpenGLFunctions();

	bool noError = true;

	// compile vertex shader
	QString vertexShaderSource(RENDERMODEL_VERTEX_SHADER);
	noError &= m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
	if (!noError)
	{
		qDebug() << m_program->log();
		return;
	}

	// compile fragment shader
	QString fragSahderSource(RENDERMODEL_FRAGMENT_SHADER);
	noError &= m_program->addShaderFromSourceCode(QOpenGLShader::Fragment,fragSahderSource);
	if (!noError)
	{
		qDebug() << m_program->log();
		return;
	}

	// link
	noError &= m_program->link();
	if (!noError)
	{
		qDebug() << m_program->log();
		return;
	}
}

COpenVROpenGLWidget::CRenderModel::~CRenderModel()
{
	Cleanup();
}

COpenVROpenGLWidget::CRenderModel* COpenVROpenGLWidget::CRenderModel::LoadModel(const QString& i_modelName, QString* o_errorMessage)
{
	COpenVROpenGLWidget::CRenderModel* pRenderModel;
	vr::RenderModel_t *pModel;
	vr::EVRRenderModelError error;
	while (true)
	{
		error = vr::VRRenderModels()->LoadRenderModel_Async(i_modelName.toStdString().c_str(), &pModel);
		if (error != vr::VRRenderModelError_Loading)
			break;

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	if (error != vr::VRRenderModelError_None)
	{
		QString errMessage("Unable to load render model %1 - %2");
		errMessage = errMessage.arg(i_modelName).arg(vr::VRRenderModels()->GetRenderModelErrorNameFromEnum(error));
		qDebug() << errMessage;
		if (o_errorMessage != nullptr)
			*o_errorMessage = errMessage;
		return nullptr; 
	}

	vr::RenderModel_TextureMap_t *pTexture;
	while (1)
	{
		error = vr::VRRenderModels()->LoadTexture_Async(pModel->diffuseTextureId, &pTexture);
		if (error != vr::VRRenderModelError_Loading)
			break;

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	if (error != vr::VRRenderModelError_None)
	{
		QString errMessage("Unable to load render texture id:%1 for render model %2");
		errMessage = errMessage.arg(pModel->diffuseTextureId).arg(i_modelName);
		qDebug() << errMessage;
		if (o_errorMessage != nullptr)
			*o_errorMessage = errMessage;
		vr::VRRenderModels()->FreeRenderModel(pModel);
		return nullptr;
	}

	pRenderModel = new CRenderModel(i_modelName);
	if (!pRenderModel->InitModel(*pModel, *pTexture))
	{
		QString errMessage("Unable to create GL model from render model %1");
		errMessage = errMessage.arg(i_modelName);
		qDebug() << errMessage;
		if (o_errorMessage != nullptr)
			*o_errorMessage = errMessage;
		delete pRenderModel;
		pRenderModel = nullptr;
	}
	vr::VRRenderModels()->FreeRenderModel(pModel);
	vr::VRRenderModels()->FreeTexture(pTexture);

	if (o_errorMessage != nullptr && pRenderModel != nullptr)
		*o_errorMessage = "Success";

	return pRenderModel;
}

bool COpenVROpenGLWidget::CRenderModel::InitModel(const vr::RenderModel_t & i_vrModel, const vr::RenderModel_TextureMap_t & i_vrDiffuseTexture)
{
	// create and bind a VAO to hold state for this model
	glGenVertexArrays(1, &m_glVertArray);
	glBindVertexArray(m_glVertArray);

	// Populate a vertex buffer
	glGenBuffers(1, &m_glVertBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, m_glVertBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vr::RenderModel_Vertex_t) * i_vrModel.unVertexCount, i_vrModel.rVertexData, GL_STATIC_DRAW);

	// Identify the components in the vertex buffer
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vr::RenderModel_Vertex_t), (void *)offsetof(vr::RenderModel_Vertex_t, vPosition));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vr::RenderModel_Vertex_t), (void *)offsetof(vr::RenderModel_Vertex_t, vNormal));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(vr::RenderModel_Vertex_t), (void *)offsetof(vr::RenderModel_Vertex_t, rfTextureCoord));

	// Create and populate the index buffer
	glGenBuffers(1, &m_glIndexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_glIndexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint16_t) * i_vrModel.unTriangleCount * 3, i_vrModel.rIndexData, GL_STATIC_DRAW);

	glBindVertexArray(0);

	// create and populate the texture
	glGenTextures(1, &m_glTexture);
	glBindTexture(GL_TEXTURE_2D, m_glTexture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, i_vrDiffuseTexture.unWidth, i_vrDiffuseTexture.unHeight,
		0, GL_RGBA, GL_UNSIGNED_BYTE, i_vrDiffuseTexture.rubTextureMapData);

	// If this renders black ask McJohn what's wrong.
	glGenerateMipmap(GL_TEXTURE_2D);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	GLfloat fLargest;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &fLargest);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, fLargest);

	glBindTexture(GL_TEXTURE_2D, 0);

	m_unVertexCount = i_vrModel.unTriangleCount * 3;

	return true;
}

void COpenVROpenGLWidget::CRenderModel::Cleanup()
{
	if (m_glVertBuffer)
	{
		glDeleteBuffers(1, &m_glIndexBuffer);
		glDeleteVertexArrays(1, &m_glVertArray);
		glDeleteBuffers(1, &m_glVertBuffer);
		m_glIndexBuffer = 0;
		m_glVertArray = 0;
		m_glVertBuffer = 0;
	}

	delete m_program;
}

void COpenVROpenGLWidget::CRenderModel::Draw(const QMatrix4x4& i_mvpMatrix)
{
	glEnable(GL_CULL_FACE);

	QMatrix4x4 scale;
	scale.scale(1.0, 1.0, -1.0);

	glUseProgram(m_program->programId());
	m_program->setUniformValue("matrix", i_mvpMatrix);
	m_program->setUniformValue("diffuse", GL_TEXTURE0);

	glBindVertexArray(m_glVertArray);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_glTexture);

	glDrawElements(GL_TRIANGLES, m_unVertexCount, GL_UNSIGNED_SHORT, 0);

	glBindVertexArray(0);

	glUseProgram(0);

	glDisable(GL_CULL_FACE);
}