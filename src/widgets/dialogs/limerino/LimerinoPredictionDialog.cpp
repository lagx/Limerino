// SPDX-License-Identifier: MIT

#include "widgets/dialogs/limerino/LimerinoPredictionDialog.hpp"

#include "common/Channel.hpp"
#include "providers/limerino/gql/LimerinoGql.hpp"
#include "providers/limerino/gql/PersistedQueries.hpp"
#include "providers/limerino/LimerinoAuth.hpp"
#include "providers/limerino/LimerinoErrors.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "singletons/Settings.hpp"
#include "widgets/splits/Split.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace chatterino::limerino {

namespace {

constexpr int MAX_OPTIONS = 10;

QStringSetting &historySetting()
{
    return getSettings()->limerinoPredictionHistory;
}

QJsonArray readHistory()
{
    QJsonParseError err{};
    const auto doc = QJsonDocument::fromJson(
        historySetting().getValue().toUtf8(), &err);
    return doc.isArray() ? doc.array() : QJsonArray{};
}

void writeHistory(const QJsonArray &arr)
{
    historySetting().setValue(QString::fromUtf8(
        QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

}  // namespace

LimerinoPredictionDialog::LimerinoPredictionDialog(Split *split)
    : BasePopup({BaseWindow::Flags::Dialog}, split)
    , split_(split)
{
    this->setWindowTitle(QStringLiteral("Limerino - predictions"));
    this->resize(420, 560);
    this->setAttribute(Qt::WA_DeleteOnClose);

    auto *root = new QVBoxLayout(this);

    // ---------------- create ----------------
    auto *createBox = new QGroupBox(QStringLiteral("Create prediction"), this);
    auto *createLayout = new QVBoxLayout(createBox);

    auto *form = new QFormLayout;
    this->titleEdit_ = new QLineEdit(createBox);
    form->addRow(QStringLiteral("Title"), this->titleEdit_);
    this->windowSpin_ = new QSpinBox(createBox);
    this->windowSpin_->setRange(30, 3600);
    this->windowSpin_->setValue(120);
    form->addRow(QStringLiteral("Window (seconds)"), this->windowSpin_);
    createLayout->addLayout(form);

    this->optionsLayout_ = new QVBoxLayout;
    createLayout->addLayout(this->optionsLayout_);

    this->addOptionRow();
    this->addOptionRow();

    auto *addRow = new QHBoxLayout;
    this->addOptionButton_ = new QPushButton(QStringLiteral("+ Add option"),
                                             createBox);
    addRow->addWidget(this->addOptionButton_);
    addRow->addStretch(1);
    createLayout->addLayout(addRow);

    this->createButton_ = new QPushButton(QStringLiteral("Create"), createBox);
    createLayout->addWidget(this->createButton_);
    root->addWidget(createBox);

    // ---------------- history ----------------
    auto *historyBox = new QGroupBox(QStringLiteral("Last 5 created"), this);
    auto *historyLayout = new QVBoxLayout(historyBox);
    this->historyBox_ = new QComboBox(historyBox);
    historyLayout->addWidget(this->historyBox_);
    this->historyUseButton_ =
        new QPushButton(QStringLiteral("Use as template"), historyBox);
    historyLayout->addWidget(this->historyUseButton_);
    root->addWidget(historyBox);

    // ---------------- active prediction ----------------
    auto *activeBox = new QGroupBox(QStringLiteral("Active prediction"), this);
    auto *activeOuter = new QVBoxLayout(activeBox);
    this->activeLabel_ = new QLabel(activeBox);
    activeOuter->addWidget(this->activeLabel_);
    this->activeLayout_ = new QVBoxLayout;
    activeOuter->addLayout(this->activeLayout_);
    auto *activeButtons = new QHBoxLayout;
    this->lockButton_ = new QPushButton(QStringLiteral("Lock"), activeBox);
    this->refreshButton_ = new QPushButton(QStringLiteral("Refresh"), activeBox);
    activeButtons->addWidget(this->lockButton_);
    activeButtons->addWidget(this->refreshButton_);
    activeButtons->addStretch(1);
    activeOuter->addLayout(activeButtons);
    root->addWidget(activeBox);
    root->addStretch(1);

    this->historyBox_->addItems(
        QStringList{QStringLiteral("(select a previous prediction)")});
    {
        const QJsonArray history = readHistory();
        for (const QJsonValue &v : history)
        {
            const QJsonObject o = v.toObject();
            QStringList options;
            for (const QJsonValue &ov : o[QStringLiteral("options")].toArray())
            {
                options.append(ov.toString());
            }
            this->historyBox_->addItem(
                QStringLiteral("%1 (%2)")
                    .arg(o[QStringLiteral("title")].toString(),
                         options.join(QStringLiteral(" / "))));
        }
    }

    // ---------------- logic ----------------
    QObject::connect(this->addOptionButton_, &QPushButton::clicked, this,
                     [this] { this->addOptionRow(); });
    QObject::connect(this->createButton_, &QPushButton::clicked, this,
                     [this] { this->submitCreate(); });
    QObject::connect(this->historyUseButton_, &QPushButton::clicked, this,
                     [this] {
                         const int index = this->historyBox_->currentIndex();
                         if (index > 0)
                         {
                             this->refillFromHistory(index - 1);
                         }
                     });
    QObject::connect(this->lockButton_, &QPushButton::clicked, this, [this] {
        if (!this->activeEventId_.isEmpty())
        {
            this->lockPrediction(this->activeEventId_);
        }
    });
    QObject::connect(this->refreshButton_, &QPushButton::clicked, this,
                     [this] { this->refreshContext(); });

    this->refreshContext();
}

void LimerinoPredictionDialog::addOptionRow(const QString &text)
{
    if (this->optionsLayout_->count() >= MAX_OPTIONS)
    {
        return;
    }
    auto *row = new QHBoxLayout;
    auto *edit = new QLineEdit(this);
    edit->setText(text);
    edit->setPlaceholderText(
        QStringLiteral("Option %1").arg(this->optionsLayout_->count() + 1));
    row->addWidget(edit, 1);
    auto *remove = new QPushButton(QStringLiteral("x"), this);
    remove->setFixedWidth(26);
    remove->setVisible(this->optionsLayout_->count() >= 2);
    row->addWidget(remove);
    this->optionsLayout_->addLayout(row);
    this->addOptionButton_->setEnabled(this->optionsLayout_->count() <
                                       MAX_OPTIONS);
    QObject::connect(remove, &QPushButton::clicked, this, [this, row] {
        if (this->optionsLayout_->count() <= 2)
        {
            return;
        }
        while (auto *item = row->takeAt(0))
        {
            delete item->widget();
            delete item;
        }
        this->optionsLayout_->removeItem(row);
        delete row;
        this->addOptionButton_->setEnabled(this->optionsLayout_->count() <
                                           MAX_OPTIONS);
    });
}

void LimerinoPredictionDialog::refillFromHistory(int index)
{
    const QJsonArray history = readHistory();
    if (index < 0 || index >= history.size())
    {
        return;
    }
    const QJsonObject o = history[index].toObject();
    this->titleEdit_->setText(o[QStringLiteral("title")].toString());
    this->windowSpin_->setValue(
        o[QStringLiteral("windowSeconds")].toInt(120));
    while (this->optionsLayout_->count() > 0)
    {
        auto *item = this->optionsLayout_->takeAt(0);
        while (auto *sub = item->takeAt(0))
        {
            delete sub->widget();
            delete sub;
        }
        delete item;
    }
    for (const QJsonValue &ov : o[QStringLiteral("options")].toArray())
    {
        this->addOptionRow(ov.toString());
    }
    this->createButton_->setFocus();
}

void LimerinoPredictionDialog::appendHistory(const QString &title,
                                             const QStringList &options,
                                             int windowSeconds)
{
    QJsonArray history = readHistory();
    QJsonArray optionsArr;
    for (const QString &o : options)
    {
        optionsArr.append(o);
    }
    history.prepend(QJsonObject{
        {QStringLiteral("title"), title},
        {QStringLiteral("options"), optionsArr},
        {QStringLiteral("windowSeconds"), windowSeconds},
    });
    while (history.size() > 5)
    {
        history.removeLast();
    }
    writeHistory(history);

    this->historyBox_->insertItem(
        1, QStringLiteral("%1 (%2)").arg(title, options.join(QStringLiteral(" / "))));
    while (this->historyBox_->count() > 6)
    {
        this->historyBox_->removeItem(this->historyBox_->count() - 1);
    }
}

void LimerinoPredictionDialog::submitCreate()
{
    auto *tchan = dynamic_cast<TwitchChannel *>(
        this->split_->getSelectedChannel().get());
    if (tchan == nullptr)
    {
        this->activeLabel_->setText(QStringLiteral("not a twitch channel"));
        return;
    }
    const QString title = this->titleEdit_->text().trimmed();
    QStringList options;
    for (int i = 0; i < this->optionsLayout_->count(); ++i)
    {
        auto *row = qobject_cast<QHBoxLayout *>(
            this->optionsLayout_->itemAt(i));
        if (row == nullptr)
        {
            continue;
        }
        auto *edit = qobject_cast<QLineEdit *>(row->itemAt(0)->widget());
        const QString t = edit != nullptr ? edit->text().trimmed() : QString();
        if (!t.isEmpty())
        {
            options.append(t);
        }
    }
    if (title.isEmpty() || options.size() < 2)
    {
        this->activeLabel_->setText(QStringLiteral(
            "need a title and at least 2 options"));
        return;
    }

    QString err;
    auto token = LimerinoAuth::resolveModerationToken(
        tchan->roomId(), tchan->getName(), &err);
    if (!token.hasToken())
    {
        this->activeLabel_->setText(err.isEmpty()
                                        ? LimerinoAuth::errors::tokenRequiredMessage(
                                              QStringLiteral("create predictions"))
                                        : err);
        return;
    }

    // Plugin's color rule: 2 options -> [BLUE, PINK]; more -> all BLUE.
    QJsonArray outcomes;
    for (int i = 0; i < options.size(); ++i)
    {
        outcomes.append(QJsonObject{
            {QStringLiteral("title"), options.at(i)},
            {QStringLiteral("color"),
             options.size() == 2 && i == 1 ? QStringLiteral("PINK")
                                           : QStringLiteral("BLUE")},
        });
    }

    const int windowSeconds = this->windowSpin_->value();
    gql::executePersisted(
        gql::PQ_CREATE_PREDICTION_EVENT,
        QJsonObject{{QStringLiteral("input"),
                     QJsonObject{{QStringLiteral("title"), title},
                                 {QStringLiteral("channelID"), tchan->roomId()},
                                 {QStringLiteral("outcomes"), outcomes},
                                 {QStringLiteral("predictionWindowSeconds"),
                                  windowSeconds}}}},
        token.token,
        [g = QPointer<LimerinoPredictionDialog>(this), title, options,
         windowSeconds](const QJsonObject &data) {
            if (!g)
            {
                return;
            }
            const QString code =
                data[QStringLiteral("createPredictionEvent")]
                    .toObject()[QStringLiteral("error")]
                    .toObject()[QStringLiteral("code")]
                    .toString();
            g->activeLabel_->setText(
                code.isEmpty() ? QStringLiteral("Successfully created prediction!")
                               : QStringLiteral("Unable to create prediction! Status: %1")
                                     .arg(code));
            if (code.isEmpty())
            {
                g->appendHistory(title, options, windowSeconds);
            }
        },
        [g = QPointer<LimerinoPredictionDialog>(this)](const gql::GqlError &e) {
            if (g)
            {
                g->activeLabel_->setText(
                    QStringLiteral("Unable to create prediction! Status: %1")
                        .arg(e.message));
            }
        });
}

void LimerinoPredictionDialog::refreshContext()
{
    auto *tchan = dynamic_cast<TwitchChannel *>(
        this->split_->getSelectedChannel().get());
    if (tchan == nullptr)
    {
        return;
    }

    QString err;
    auto token = LimerinoAuth::resolveModerationToken(
        tchan->roomId(), tchan->getName(), &err);
    if (!token.hasToken())
    {
        this->activeLabel_->setText(err.isEmpty()
                                        ? LimerinoAuth::errors::tokenRequiredMessage(
                                              QStringLiteral("manage predictions"))
                                        : err);
        return;
    }

    gql::executePersisted(
        gql::PQ_CHANNEL_POINTS_PREDICTION_CONTEXT,
        QJsonObject{{QStringLiteral("count"), 1},
                    {QStringLiteral("channelLogin"), tchan->getName()}},
        token.token,
        [g = QPointer<LimerinoPredictionDialog>(this), this](
            const QJsonObject &data) {
            if (!g)
            {
                return;
            }
            const QJsonObject channel =
                data[QStringLiteral("community")].toObject()[
                    QStringLiteral("channel")].toObject();
            const QJsonArray active =
                channel[QStringLiteral("activePredictionEvents")].toArray();
            const QJsonArray locked =
                channel[QStringLiteral("lockedPredictionEvents")].toArray();
            const QJsonObject event =
                (!active.isEmpty() ? active.first()
                                   : (!locked.isEmpty() ? locked.first()
                                                        : QJsonObject{}))
                    .toObject();
            if (event.isEmpty())
            {
                this->activeEventId_.clear();
                this->lockButton_->setEnabled(false);
                this->activeLabel_->setText(
                    QStringLiteral("No active prediction."));
                return;
            }

            this->activeEventId_ = event[QStringLiteral("id")].toString();
            const bool isLocked = locked.contains(event);
            this->lockButton_->setEnabled(!isLocked);

            QString text =
                (isLocked ? QStringLiteral("[locked] ")
                          : QStringLiteral("[active] ")) +
                event[QStringLiteral("title")].toString() +
                QStringLiteral(" (by ") +
                event[QStringLiteral("createdBy")].toObject()[
                    QStringLiteral("displayName")].toString() +
                QStringLiteral(", window %1s)")
                    .arg(event[QStringLiteral("predictionWindowSeconds")]
                             .toInt());
            this->activeLabel_->setText(text);

            // Replace outcome action buttons
            while (auto *item = this->activeLayout_->takeAt(0))
            {
                while (auto *sub = item->takeAt(0))
                {
                    delete sub->widget();
                    delete sub;
                }
                delete item;
            }
            const QJsonArray outcomes =
                event[QStringLiteral("outcomes")].toArray();
            const QString eventId = this->activeEventId_;
            for (const QJsonValue &ov : outcomes)
            {
                const QJsonObject outcome = ov.toObject();
                auto *row = new QHBoxLayout;
                row->addWidget(new QLabel(QStringLiteral(
                                     "%1 (%2 points)")
                                     .arg(outcome[QStringLiteral("title")]
                                              .toString())
                                     .arg(outcome[QStringLiteral("totalPoints")]
                                              .toInt())));
                auto *payButton =
                    new QPushButton(QStringLiteral("Payout"), this);
                const QString outcomeId =
                    outcome[QStringLiteral("id")].toString();
                const QString outcomeTitle =
                    outcome[QStringLiteral("title")].toString();
                QObject::connect(payButton, &QPushButton::clicked, this,
                                 [this, eventId, outcomeId, outcomeTitle] {
                                     const auto answer = QMessageBox::question(
                                         this, QStringLiteral("Pay out"),
                                         QStringLiteral("Resolve this prediction "
                                                        "paying outcome \"%1\"?")
                                             .arg(outcomeTitle));
                                     if (answer == QMessageBox::Yes)
                                     {
                                         this->payoutPrediction(eventId,
                                                                outcomeId);
                                     }
                                 });
                row->addWidget(payButton);
                row->addStretch(1);
                this->activeLayout_->addLayout(row);
            }
        },
        [g = QPointer<LimerinoPredictionDialog>(this)](const gql::GqlError &e) {
            if (g)
            {
                g->activeLabel_->setText(e.message);
            }
        });
}

void LimerinoPredictionDialog::lockPrediction(const QString &eventId)
{
    QString err;
    auto *tchan = dynamic_cast<TwitchChannel *>(
        this->split_->getSelectedChannel().get());
    auto token = tchan ? LimerinoAuth::resolveModerationToken(
                             tchan->roomId(), tchan->getName(), &err)
                       : LimerinoAuth::LimerinoAuthToken{};
    if (!token.hasToken())
    {
        this->activeLabel_->setText(err);
        return;
    }
    gql::executePersisted(
        gql::PQ_LOCK_PREDICTION,
        QJsonObject{{QStringLiteral("input"),
                     QJsonObject{{QStringLiteral("id"), eventId}}}},
        token.token,
        [g = QPointer<LimerinoPredictionDialog>(this)](
            const QJsonObject & /*data*/) {
            if (g)
            {
                g->activeLabel_->setText(
                    QStringLiteral("Successfully locked prediction!"));
                g->refreshContext();
            }
        },
        [g = QPointer<LimerinoPredictionDialog>(this)](const gql::GqlError &e) {
            if (g)
            {
                g->activeLabel_->setText(
                    QStringLiteral("Unable to lock prediction! %1").arg(e.message));
            }
        });
}

void LimerinoPredictionDialog::payoutPrediction(const QString &eventId,
                                                const QString &outcomeId)
{
    QString err;
    auto *tchan = dynamic_cast<TwitchChannel *>(
        this->split_->getSelectedChannel().get());
    auto token = tchan ? LimerinoAuth::resolveModerationToken(
                             tchan->roomId(), tchan->getName(), &err)
                       : LimerinoAuth::LimerinoAuthToken{};
    if (!token.hasToken())
    {
        this->activeLabel_->setText(err);
        return;
    }
    gql::executePersisted(
        gql::PQ_RESOLVE_PREDICTION,
        QJsonObject{{QStringLiteral("input"),
                     QJsonObject{{QStringLiteral("eventID"), eventId},
                                 {QStringLiteral("outcomeID"), outcomeId}}}},
        token.token,
        [g = QPointer<LimerinoPredictionDialog>(this)](
            const QJsonObject & /*data*/) {
            if (g)
            {
                g->activeLabel_->setText(
                    QStringLiteral("Successfully resolved prediction!"));
                g->refreshContext();
            }
        },
        [g = QPointer<LimerinoPredictionDialog>(this)](const gql::GqlError &e) {
            if (g)
            {
                g->activeLabel_->setText(
                    QStringLiteral("Unable to resolve prediction! %1")
                        .arg(e.message));
            }
        });
}

}  // namespace chatterino::limerino
