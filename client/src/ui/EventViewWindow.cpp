#include "EventViewWindow.h"
#include "EventsModel.h"
#include "EventTagsView.h"
#include "EventTagsModel.h"
#include "EventCommentsWidget.h"
#include "ExpandingTextEdit.h"
#include "core/DVRServer.h"
#include <QBoxLayout>
#include <QSplitter>
#include <QFrame>
#include <QLabel>
#include <QSlider>
#include <QTextDocument>
#include <QCloseEvent>
#include <QSettings>
#include <QListView>
#include <QTextEdit>
#include <QToolButton>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>

EventViewWindow::EventViewWindow(QWidget *parent)
    : QWidget(parent, Qt::Window), m_event(0)
{
    setWindowTitle(tr("Bluecherry DVR - Event Playback"));

    QBoxLayout *layout = new QVBoxLayout(this);
    m_splitter = new QSplitter(Qt::Horizontal, this);
    layout->addWidget(m_splitter);

    m_splitter->addWidget(createInfoArea());
    m_splitter->addWidget(createPlaybackArea());
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setChildrenCollapsible(false);

    QSettings settings;
    restoreGeometry(settings.value(QLatin1String("ui/eventView/geometry")).toByteArray());
    m_splitter->restoreState(settings.value(QLatin1String("ui/eventView/splitState")).toByteArray());
}

void EventViewWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings;
    settings.setValue(QLatin1String("ui/eventView/geometry"), saveGeometry());
    settings.setValue(QLatin1String("ui/eventView/splitState"), m_splitter->saveState());
    QWidget::closeEvent(event);
}

void EventViewWindow::setEvent(EventData *event)
{
    m_event = event;

    m_infoLabel->setText(tr("<b>%2</b> (%1)<br><br>%3 (%4)<br>%5")
                         .arg(Qt::escape(event->uiServer()))
                         .arg(Qt::escape(event->uiLocation()))
                         .arg(Qt::escape(event->uiType()))
                         .arg(Qt::escape(event->uiLevel()))
                         .arg(event->date.toString()));
}

QWidget *EventViewWindow::createInfoArea()
{
    QFrame *container = new QFrame;
    container->setFrameStyle(QFrame::Plain);
    container->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);

    QBoxLayout *layout = new QVBoxLayout(container);
    layout->setMargin(0);

    m_infoLabel = new QLabel;
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    layout->addWidget(m_infoLabel);

    QFrame *line = new QFrame;
    line->setFrameStyle(QFrame::Sunken | QFrame::HLine);
    layout->addWidget(line);

    QLabel *title = new QLabel(tr("Tags:"));
    title->setStyleSheet(QLatin1String("font-weight:bold"));
    layout->addWidget(title);

    m_tagsView = new EventTagsView;
    m_tagsView->setModel(new EventTagsModel(m_tagsView));
    m_tagsView->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_tagsView->setFrameStyle(QFrame::NoFrame);
    layout->addWidget(m_tagsView);

    m_tagsInput = new QComboBox;
    m_tagsInput->setEditable(true);
    m_tagsInput->setInsertPolicy(QComboBox::NoInsert);
#if QT_VERSION >= 0x040700
    m_tagsInput->lineEdit()->setPlaceholderText(tr("Add a tag"));
#endif
    layout->addWidget(m_tagsInput);

    line = new QFrame;
    line->setFrameStyle(QFrame::Sunken | QFrame::HLine);
    layout->addWidget(line);

    title = new QLabel(tr("Comments:"));
    title->setStyleSheet(QLatin1String("font-weight:bold"));
    layout->addWidget(title);

    m_commentsArea = new EventCommentsWidget;
    m_commentsArea->setFrameStyle(QFrame::NoFrame);
    m_commentsArea->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_commentsArea->setMinimumWidth(160);
    m_commentsArea->setCursor(Qt::ArrowCursor);
    layout->addWidget(m_commentsArea);

    m_commentInput = new ExpandingTextEdit;
    m_commentInput->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_commentInput->setTabChangesFocus(true);
    connect(m_commentInput, SIGNAL(textChanged()), SLOT(commentInputChanged()));
    layout->addWidget(m_commentInput);

    m_commentBtn = new QPushButton(tr("Add comment"));
    m_commentBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_commentBtn->hide();
    connect(m_commentBtn, SIGNAL(clicked()), SLOT(postComment()));
    layout->addWidget(m_commentBtn, 0, Qt::AlignRight | Qt::AlignVCenter);

    /* For testing purposes */
    m_commentsArea->appendComment(QLatin1String("Author"), QDateTime::currentDateTime(),
                                  QLatin1String("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed in "
                                                "nisi quis augue ultrices sagittis. Fusce porttitor sagittis urna, "
                                                "ac facilisis sem aliquet sit amet."));

    m_commentsArea->appendComment(QLatin1String("Second Author"), QDateTime::currentDateTime(),
                                  QLatin1String("Ok."));

    layout->addStretch();

    return container;
}

QWidget *EventViewWindow::createPlaybackArea()
{
    QWidget *container = new QWidget;
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QBoxLayout *layout = new QVBoxLayout(container);
    layout->setMargin(0);

    QWidget *videoArea = new QWidget;
    videoArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    videoArea->setMinimumSize(320, 240);
    videoArea->setAutoFillBackground(true);
    videoArea->setStyleSheet(QLatin1String("background-color:black;"));
    layout->addWidget(videoArea);

    QSlider *videoSlider = new QSlider(Qt::Horizontal);
    videoSlider->setEnabled(false);
    layout->addWidget(videoSlider);

    QBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->setSpacing(3);
    layout->addLayout(btnLayout);

    QToolButton *playBtn = new QToolButton;
    playBtn->setText(QString(QChar(0x25BA)));
    btnLayout->addWidget(playBtn);

    btnLayout->addSpacing(9);

    QToolButton *slowBtn = new QToolButton;
    slowBtn->setText(QString(2, QChar(0x25C4)));
    btnLayout->addWidget(slowBtn);

    QToolButton *fastBtn = new QToolButton;
    fastBtn->setText(QString(2, QChar(0x25BA)));
    btnLayout->addWidget(fastBtn);

    btnLayout->addSpacing(9);

    QToolButton *restartBtn = new QToolButton;
    restartBtn->setText(QString::fromUtf8("|\xe2\x97\x84"));
    btnLayout->addWidget(restartBtn);

    btnLayout->addStretch();
    return container;
}

void EventViewWindow::commentInputChanged()
{
    if (m_commentInput->document()->isEmpty())
    {
        m_commentBtn->setEnabled(false);
        m_commentBtn->hide();
    }
    else
    {
        m_commentBtn->setEnabled(true);
        m_commentBtn->show();
    }
}

void EventViewWindow::postComment()
{
    QString text = m_commentInput->toPlainText();
    if (text.isEmpty())
        return;

    m_commentsArea->appendComment(QLatin1String("Username"), QDateTime::currentDateTime(),
                                  text);

    m_commentInput->clear();
}
