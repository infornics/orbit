#pragma once

#include <QWidget>

namespace Orbit {

class CodeEditor;

class LineNumberArea : public QWidget {
    Q_OBJECT

public:
    explicit LineNumberArea(CodeEditor *editor);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    CodeEditor *m_codeEditor;
};

} // namespace Orbit
