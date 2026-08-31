/*	LSculpt: Studs-out LEGO(R) Sculpture

	This file (ldrawpreviewwidget.h) is part of LSculpt.

	Qt-native replacement for the Windows-only LDVLib.dll preview: embeds
	LDView's LDrawModelViewer engine directly in a QOpenGLWidget instead of
	going through a raw HWND/wglMakeCurrent C API. Modeled on
	ldview/QT/ModelViewerWidget.{h,cpp}, trimmed to the load/render/basic-
	view-control surface LSculpt actually uses (LDVInit/LDVSetFilename/
	LDVLoadModel/LDVSetSize/LDVSetSeamWidth/LDVSetTextureStuds/
	LDVSetLDrawDir from the old LDVLib.h). */
#pragma once

#include <QOpenGLWidget>
#include <QString>

#include <LDLib/LDInputHandler.h>

class LDrawModelViewer;
class LDPreviewAlertHandler;

class LDrawPreviewWidget : public QOpenGLWidget
{
    Q_OBJECT
public:
    explicit LDrawPreviewWidget(QWidget *parent = nullptr);
    ~LDrawPreviewWidget() override;

    // Replacement for LDVSetLDrawDir(const char *path)
    static void setLDrawDir(const QString &path);

    // Replacement for LDVSetFilename(pLDV, filename) + LDVLoadModel(pLDV, resetViewpoint)
    void setFilename(const QString &filename);
    bool loadModel(bool resetViewpoint = true);

    // Replacement for the corresponding LDVSet*/LDVGet* rendering-option pairs
    void setSeamWidth(float value);
    void setTextureStuds(bool value);

    // Replacement for LDVResetView / LDVZoomToFit
    void resetView();
    void zoomToFit();

    // Replacement for LDVEnableInput / LDVIsInputEnabled
    void setInputEnabled(bool enable) { inputEnabled = enable; }
    bool isInputEnabled() const { return inputEnabled; }

    LDrawModelViewer *getModelViewer() { return modelViewer; }

protected:
    // QOpenGLWidget overrides -- LDVLib did this by hand-pumping WM_PAINT/
    // wglMakeCurrent; Qt owns the GL context lifecycle here instead.
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;

    // Route Qt input events into LDInputHandler, replacing LDVLib.cpp's
    // doButtonDown/doButtonUp/doMouseMove/doMouseWheel WM_* handlers.
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    static TCULong convertKeyModifiers(Qt::KeyboardModifiers osModifiers);
    static LDInputHandler::MouseButton convertMouseButton(Qt::MouseButton button);
    static LDInputHandler::KeyCode convertKeyCode(int qtKey);

    LDrawModelViewer *modelViewer;
    LDInputHandler *inputHandler;
    LDPreviewAlertHandler *alertHandler;
    QString filenameValue;
    bool inputEnabled;
};
