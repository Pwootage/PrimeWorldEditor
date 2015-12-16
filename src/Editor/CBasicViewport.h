#ifndef CEDITORGLWIDGET_H
#define CEDITORGLWIDGET_H

#include <Common/CTimer.h>
#include <Math/CRay.h>
#include <Math/CVector2i.h>
#include <Math/CVector2f.h>
#include <Core/Render/CRenderer.h>
#include <Core/Render/SViewInfo.h>

#include <QOpenGLWidget>
#include <QMouseEvent>
#include <QPoint>
#include <QTimer>

#include <GL/glew.h>

class CBasicViewport : public QOpenGLWidget
{
    Q_OBJECT

protected:
    // Render
    CCamera mCamera;
    CTimer mFrameTimer;
    double mLastDrawTime;
    SViewInfo mViewInfo;

    // Cursor settings
    QCursor mCursorState;
    bool mCursorVisible;

    // Input
    QPoint mLastMousePos;
    bool mMouseMoved;
    CTimer mMoveTimer;
    int mButtonsPressed; // int container for EMouseInputs flags
    int mKeysPressed;    // int container for EKeyInputs flags

public:
    explicit CBasicViewport(QWidget *pParent = 0);
    ~CBasicViewport();
    void initializeGL();
    void paintGL();
    void resizeGL(int w, int h);
    void mousePressEvent(QMouseEvent *pEvent);
    void mouseReleaseEvent(QMouseEvent *pEvent);
    void mouseMoveEvent(QMouseEvent *pEvent);
    void wheelEvent(QWheelEvent *pEvent);
    virtual void keyPressEvent(QKeyEvent *pEvent);
    virtual void keyReleaseEvent(QKeyEvent *pEvent);
    void focusOutEvent(QFocusEvent *pEvent);
    void contextMenuEvent(QContextMenuEvent *pEvent);

    void SetGameMode(bool Enabled);
    void SetCursorState(const QCursor& Cursor);
    void SetCursorVisible(bool visible);
    bool IsCursorVisible();
    bool IsMouseInputActive();
    bool IsKeyboardInputActive();
    CCamera& Camera();
    CRay CastRay();
    CVector2f MouseDeviceCoordinates();
    double LastRenderDuration();

public slots:
    void ProcessInput();
    void Render();

protected slots:
    virtual void CheckUserInput() {}
    virtual void Paint() {}
    virtual void ContextMenu(QContextMenuEvent* /*pEvent*/) {}
    virtual void OnResize() {}
    virtual void OnMouseClick(QMouseEvent* /*pEvent*/) {}
    virtual void OnMouseRelease(QMouseEvent* /*pEvent*/) {}

private:
    void DrawAxes();
};

#endif