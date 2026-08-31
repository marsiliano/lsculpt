/*	LSculpt: Studs-out LEGO(R) Sculpture

	This file (ldrawpreviewwidget.cpp) is part of LSculpt.  See
	ldrawpreviewwidget.h for context. */

// TRE/TREGL.h unconditionally #defines GL_GLEXT_PROTOTYPES on non-Windows
// platforms, which changes whether its own vendored glext.h typedefs the
// legacy PFNGL*ARBPROC/EXTPROC function-pointer types that
// TRE/TREGLExtensions.h's class members use unconditionally. Include the
// TRE GL headers first, before anything Qt-OpenGL-related (including our
// own header, which derives from QOpenGLWidget) gets a chance to pull in
// the platform's own gl3.h and race that guard.
#include <TRE/TREGL.h>
#include <TRE/TREGLExtensions.h>

#include "ldrawpreviewwidget.h"

#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

#include <TCFoundation/TCAlert.h>
#include <TCFoundation/TCAlertManager.h>
#include <TCFoundation/TCObject.h>
#include <LDLib/LDPreferences.h>
#include <LDLib/LDrawModelViewer.h>
#include <LDLoader/LDLModel.h>

// Replaces LDVLib.cpp's Win32 AlertHandler (SetCapture/ReleaseCapture on a
// raw HWND, RedrawWindow via a WM_TIMER) with the Qt equivalents
// (QWidget::grabMouse/releaseMouse, QWidget::update). Same three alert
// classes, same dispatch logic -- see LDVLib.cpp's AlertHandler for the
// original.
class LDPreviewAlertHandler : public TCObject
{
public:
	explicit LDPreviewAlertHandler(LDrawPreviewWidget *widget)
		: widget(widget)
	{
		TCAlertManager::registerHandler(LDInputHandler::captureAlertClass(), this,
			(TCAlertCallback)&LDPreviewAlertHandler::captureAlertCallback);
		TCAlertManager::registerHandler(LDInputHandler::releaseAlertClass(), this,
			(TCAlertCallback)&LDPreviewAlertHandler::releaseAlertCallback);
		TCAlertManager::registerHandler(LDrawModelViewer::redrawAlertClass(), this,
			(TCAlertCallback)&LDPreviewAlertHandler::redrawAlertCallback);
	}

protected:
	void captureAlertCallback(TCAlert *alert)
	{
		if (alert->getSender() == widget->getModelViewer()->getInputHandler())
		{
			widget->grabMouse();
		}
	}

	void releaseAlertCallback(TCAlert *alert)
	{
		if (alert->getSender() == widget->getModelViewer()->getInputHandler())
		{
			widget->releaseMouse();
		}
	}

	void redrawAlertCallback(TCAlert *alert)
	{
		if (alert->getSender() == widget->getModelViewer())
		{
			widget->update();
		}
	}

	LDrawPreviewWidget *widget;
};

LDrawPreviewWidget::LDrawPreviewWidget(QWidget *parent)
	: QOpenGLWidget(parent)
	, modelViewer(new LDrawModelViewer(100, 100))
	, alertHandler(nullptr)
	, inputEnabled(true)
{
	inputHandler = modelViewer->getInputHandler();

	LDPreferences *prefs = new LDPreferences(modelViewer);
	prefs->loadSettings();
	prefs->applySettings();
	prefs->release();

	alertHandler = new LDPreviewAlertHandler(this);

	setFocusPolicy(Qt::StrongFocus);
}

LDrawPreviewWidget::~LDrawPreviewWidget()
{
	TCObject::release(alertHandler);
	TCObject::release(modelViewer);
}

void LDrawPreviewWidget::setLDrawDir(const QString &path)
{
	LDLModel::setLDrawDir(path.toUtf8().constData());
}

void LDrawPreviewWidget::setFilename(const QString &filename)
{
	filenameValue = filename;
}

bool LDrawPreviewWidget::loadModel(bool resetViewpoint)
{
	// Mirrors LDVLoadModel: chdir into the model's directory before loading
	// (LDraw model files reference sibling/relative part paths), then
	// restore the previous working directory afterward.
	QFileInfo info(filenameValue);
	QString origDir = QDir::currentPath();
	bool dirChanged = false;
	bool result = false;

	if (info.exists())
	{
		QDir::setCurrent(info.absolutePath());
		dirChanged = true;
		modelViewer->setFilename(info.absoluteFilePath().toUtf8().constData());
	}

	makeCurrent();
	if (modelViewer->loadModel(resetViewpoint))
	{
		result = true;
		update();
	}
	doneCurrent();

	if (dirChanged)
	{
		QDir::setCurrent(origDir);
	}
	return result;
}

void LDrawPreviewWidget::setSeamWidth(float value)
{
	modelViewer->setSeamWidth(value);
}

void LDrawPreviewWidget::setTextureStuds(bool value)
{
	modelViewer->setTextureStuds(value);
}

void LDrawPreviewWidget::resetView()
{
	modelViewer->resetView();
	update();
}

void LDrawPreviewWidget::zoomToFit()
{
	modelViewer->zoomToFit();
	update();
}

void LDrawPreviewWidget::initializeGL()
{
	TREGLExtensions::setup();
}

void LDrawPreviewWidget::resizeGL(int width, int height)
{
	modelViewer->setWidth(width);
	modelViewer->setHeight(height);
	glViewport(0, 0, width, height);
}

void LDrawPreviewWidget::paintGL()
{
	modelViewer->update();
}

// static
TCULong LDrawPreviewWidget::convertKeyModifiers(Qt::KeyboardModifiers osModifiers)
{
	TCULong retValue = 0;

	if (osModifiers & Qt::ShiftModifier)
	{
		retValue |= LDInputHandler::MKShift;
	}
	if (osModifiers & Qt::ControlModifier)
	{
		retValue |= LDInputHandler::MKControl;
	}
	return retValue;
}

// static
LDInputHandler::MouseButton LDrawPreviewWidget::convertMouseButton(Qt::MouseButton button)
{
	switch (button)
	{
	case Qt::LeftButton:
		return LDInputHandler::MBLeft;
	case Qt::RightButton:
		return LDInputHandler::MBRight;
	case Qt::MiddleButton:
		return LDInputHandler::MBMiddle;
	default:
		return LDInputHandler::MBUnknown;
	}
}

// static
LDInputHandler::KeyCode LDrawPreviewWidget::convertKeyCode(int qtKey)
{
	if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
	{
		return (LDInputHandler::KeyCode)(qtKey - Qt::Key_A + LDInputHandler::KCA);
	}
	switch (qtKey)
	{
	case Qt::Key_Up:
		return LDInputHandler::KCUp;
	case Qt::Key_Down:
		return LDInputHandler::KCDown;
	case Qt::Key_Left:
		return LDInputHandler::KCLeft;
	case Qt::Key_Right:
		return LDInputHandler::KCRight;
	case Qt::Key_Space:
		return LDInputHandler::KCSpace;
	case Qt::Key_PageUp:
		return LDInputHandler::KCPageUp;
	case Qt::Key_PageDown:
		return LDInputHandler::KCPageDown;
	case Qt::Key_Home:
		return LDInputHandler::KCHome;
	case Qt::Key_End:
		return LDInputHandler::KCEnd;
	case Qt::Key_Insert:
		return LDInputHandler::KCInsert;
	case Qt::Key_Delete:
		return LDInputHandler::KCDelete;
	case Qt::Key_Escape:
		return LDInputHandler::KCEscape;
	default:
		return LDInputHandler::KCUnknown;
	}
}

void LDrawPreviewWidget::mousePressEvent(QMouseEvent *event)
{
	if (!inputEnabled ||
		!inputHandler->mouseDown(convertKeyModifiers(event->modifiers()),
			convertMouseButton(event->button()),
			event->globalPosition().x(), event->globalPosition().y()))
	{
		event->ignore();
		return;
	}
	setFocus();
}

void LDrawPreviewWidget::mouseReleaseEvent(QMouseEvent *event)
{
	if (!inputEnabled ||
		!inputHandler->mouseUp(convertKeyModifiers(event->modifiers()),
			convertMouseButton(event->button()),
			event->globalPosition().x(), event->globalPosition().y()))
	{
		event->ignore();
	}
}

void LDrawPreviewWidget::mouseMoveEvent(QMouseEvent *event)
{
	if (!inputEnabled ||
		!inputHandler->mouseMove(convertKeyModifiers(event->modifiers()),
			event->globalPosition().x(), event->globalPosition().y()))
	{
		event->ignore();
	}
}

void LDrawPreviewWidget::wheelEvent(QWheelEvent *event)
{
	if (!inputEnabled ||
		!inputHandler->mouseWheel(convertKeyModifiers(event->modifiers()),
			(TCFloat)event->angleDelta().y() * 0.5f))
	{
		event->ignore();
	}
}

void LDrawPreviewWidget::keyPressEvent(QKeyEvent *event)
{
	if (inputEnabled &&
		inputHandler->keyDown(convertKeyModifiers(event->modifiers()),
			convertKeyCode(event->key())))
	{
		event->accept();
	}
	else
	{
		event->ignore();
	}
	QOpenGLWidget::keyPressEvent(event);
}

void LDrawPreviewWidget::keyReleaseEvent(QKeyEvent *event)
{
	if (inputEnabled &&
		inputHandler->keyUp(convertKeyModifiers(event->modifiers()),
			convertKeyCode(event->key())))
	{
		event->accept();
	}
	else
	{
		event->ignore();
	}
	QOpenGLWidget::keyReleaseEvent(event);
}
