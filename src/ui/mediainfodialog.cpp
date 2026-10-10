#include "mediainfodialog.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QMouseEvent>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

#include "guiutils.h"
#include "mediainfoservice.h"

namespace {
// FontAwesome 关闭图标
const QChar kCloseIcon(0xf00d);
constexpr int kDialogWidth = 420;
// 实时信息刷新间隔：缓冲统计变化频率不高，0.5s 足够平滑
constexpr int kRefreshIntervalMs = 500;
} // namespace

MediaInfoDialog::MediaInfoDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName("MediaInfoDialog");
    setWindowTitle(tr("播放信息"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setModal(true);
    setFixedWidth(kDialogWidth);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // 标题栏：可拖动，样式对齐主窗口标题栏
    _titleBar = new QWidget(this);
    _titleBar->setObjectName("MediaInfoTitleBar");
    auto *titleLayout = new QHBoxLayout(_titleBar);
    titleLayout->setContentsMargins(14, 8, 8, 8);
    titleLayout->setSpacing(6);

    auto *titleLabel = new QLabel(tr("播放信息"), _titleBar);
    titleLabel->setObjectName("MediaInfoTitle");
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();

    auto *closeButton = new QPushButton(_titleBar);
    closeButton->setObjectName("MediaInfoCloseBtn");
    closeButton->setToolTip(tr("关闭"));
    closeButton->setCursor(Qt::PointingHandCursor);
    GuiUtils::SetIcon(closeButton, 10, kCloseIcon);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    titleLayout->addWidget(closeButton);

    rootLayout->addWidget(_titleBar);

    auto *separator = new QFrame(this);
    separator->setObjectName("MediaInfoSeparator");
    separator->setFrameShape(QFrame::NoFrame);
    separator->setFixedHeight(1);
    rootLayout->addWidget(separator);

    auto *content = new QWidget(this);
    content->setObjectName("MediaInfoContent");
    _fieldLayout = new QGridLayout(content);
    _fieldLayout->setContentsMargins(16, 12, 16, 14);
    _fieldLayout->setHorizontalSpacing(18);
    _fieldLayout->setVerticalSpacing(9);
    _fieldLayout->setColumnStretch(0, 0);
    _fieldLayout->setColumnStretch(1, 1);
    rootLayout->addWidget(content);

    setStyleSheet(GuiUtils::LoadQss(":/res/qss/mediainfo.css"));

    _refreshTimer.setInterval(kRefreshIntervalMs);
    connect(&_refreshTimer, &QTimer::timeout, this, [this]() {
        if (_infoProvider) {
            setMediaInfo(_infoProvider());
        }
    });

    setMediaInfo(MediaInfo{});
}

void MediaInfoDialog::setInfoProvider(std::function<MediaInfo()> provider)
{
    _infoProvider = std::move(provider);
    if (_infoProvider) {
        // 先取一次，避免面板打开到第一次超时之间显示旧数据
        setMediaInfo(_infoProvider());
        _refreshTimer.start();
    } else {
        _refreshTimer.stop();
    }
}

void MediaInfoDialog::setMediaInfo(const MediaInfo &info)
{
    if (!info.valid) {
        if (_showingPlaceholder) {
            return;
        }
        clearRows();
        auto *hint = new QLabel(tr("当前没有正在播放的媒体。"), this);
        hint->setObjectName("MediaInfoHint");
        hint->setWordWrap(true);
        _fieldLayout->addWidget(hint, 0, 0, 1, 2);
        _showingPlaceholder = true;
        adjustSize();
        return;
    }

    applyFields(MediaInfoService::describe(info));
}

void MediaInfoDialog::applyFields(const QList<MediaInfoField> &fields)
{
    if (_showingPlaceholder || _valueLabels.size() != fields.size()) {
        clearRows();
        buildRows(fields);
        adjustSize();
        return;
    }
    refreshValues(fields);
}

void MediaInfoDialog::clearRows()
{
    /*
     * 从布局中移除的控件不会自动隐藏，必须立即销毁，
     * 否则上一次的内容会和本次的内容同时显示。
     */
    while (QLayoutItem *item = _fieldLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    _valueLabels.clear();
    _showingPlaceholder = false;
}

void MediaInfoDialog::buildRows(const QList<MediaInfoField> &fields)
{
    int row = 0;
    for (const MediaInfoField &field : fields) {
        auto *label = new QLabel(field.label, this);
        label->setObjectName("MediaInfoFieldLabel");
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        auto *value = new QLabel(field.value, this);
        value->setObjectName("MediaInfoFieldValue");
        value->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        value->setWordWrap(true);

        _fieldLayout->addWidget(label, row, 0);
        _fieldLayout->addWidget(value, row, 1);
        _valueLabels.append(value);
        ++row;
    }
}

void MediaInfoDialog::refreshValues(const QList<MediaInfoField> &fields)
{
    bool changed = false;
    const int count = qMin(fields.size(), _valueLabels.size());
    for (int i = 0; i < count; ++i) {
        QLabel *value = _valueLabels.at(i);
        if (value->text() != fields.at(i).value) {
            value->setText(fields.at(i).value);
            changed = true;
        }
    }

    // 文字长度变化可能改变换行行数，仅在高度确实需要变化时重新贴合，避免面板抖动
    if (changed && sizeHint().height() != height()) {
        adjustSize();
    }
}

void MediaInfoDialog::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton
        && _titleBar->geometry().contains(event->position().toPoint())) {
        _dragging = true;
        _dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void MediaInfoDialog::mouseMoveEvent(QMouseEvent *event)
{
    if (_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - _dragOffset);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void MediaInfoDialog::mouseReleaseEvent(QMouseEvent *event)
{
    _dragging = false;
    QDialog::mouseReleaseEvent(event);
}

void MediaInfoDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);

    // 首次显示时居中到父窗口，避免弹窗落到屏幕角落
    if (!_centered && parentWidget()) {
        _centered = true;
        const QPoint center = parentWidget()->mapToGlobal(parentWidget()->rect().center());
        move(center.x() - width() / 2, center.y() - height() / 2);
    }
}
