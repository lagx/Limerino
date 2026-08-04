// SPDX-License-Identifier: MIT

#include "widgets/dialogs/limerino/LimerinoAutoActionEditor.hpp"

#include "providers/limerino/autoactions/LimerinoAutoAction.hpp"
#include "providers/limerino/autoactions/AutoActionPlaceholders.hpp"
#include "providers/limerino/matcher/LimerinoMatcher.hpp"
#include "providers/limerino/matcher/LimerinoMatcherValidation.hpp"
#include "util/LayoutHelper.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace chatterino::limerino {

namespace {

constexpr const char *kScopeAllExcept = "everywhere except...";
constexpr const char *kScopeOnly = "only in...";

}  // namespace

LimerinoAutoActionEditor::LimerinoAutoActionEditor(QWidget *parent,
                                                   const LimerinoAutoAction &existing)
    : BasePopup({BaseWindow::Flags::Dialog}, parent)
    , id_(existing.id)
{
    this->setWindowTitle(existing.name.isEmpty()
                             ? QStringLiteral("New auto action (Limerino)")
                             : QStringLiteral("Edit auto action - %1 (Limerino)")
                                   .arg(existing.name));

    auto *root = new QVBoxLayout(this);
    auto *form = new QFormLayout();
    root->addLayout(form);

    // Name + enable
    this->nameEdit_ = new QLineEdit(existing.name, this);
    this->nameEdit_->setPlaceholderText(QStringLiteral("My rule"));
    form->addRow(QStringLiteral("Name"), this->nameEdit_);

    this->enabledCheck_ = new QCheckBox(QStringLiteral("Enabled"), this);
    this->enabledCheck_->setChecked(existing.enabled);
    form->addRow(QString(), this->enabledCheck_);

    // Content matcher
    {
        auto *row = new QHBoxLayout();
        row->setContentsMargins(0, 0, 0, 0);
        this->contentEdit_ = new QLineEdit(existing.content.pattern, this);
        this->contentRegex_ = new QCheckBox(QStringLiteral("Regex"), this);
        this->contentRegex_->setChecked(existing.content.isRegex);
        this->contentCase_ = new QCheckBox(QStringLiteral("Case sensitive"), this);
        this->contentCase_->setChecked(existing.content.caseSensitive);
        row->addWidget(this->contentEdit_, 1);
        row->addWidget(this->contentRegex_);
        row->addWidget(this->contentCase_);
        form->addRow(QStringLiteral("Message matcher"), row);

        this->contentError_ = new QLabel(this);
        this->contentError_->setStyleSheet(QStringLiteral("color: #d9534f;"));
        this->contentError_->setWordWrap(true);
        form->addRow(QString(), this->contentError_);
    }

    // Sender matcher
    {
        auto *row = new QHBoxLayout();
        row->setContentsMargins(0, 0, 0, 0);
        this->senderEdit_ = new QLineEdit(existing.sender.pattern, this);
        this->senderRegex_ = new QCheckBox(QStringLiteral("Regex"), this);
        this->senderRegex_->setChecked(existing.sender.isRegex);
        this->senderCase_ = new QCheckBox(QStringLiteral("Case sensitive"), this);
        this->senderCase_->setChecked(existing.sender.caseSensitive);
        row->addWidget(this->senderEdit_, 1);
        row->addWidget(this->senderRegex_);
        row->addWidget(this->senderCase_);
        form->addRow(QStringLiteral("Sender matcher"), row);

        this->senderError_ = new QLabel(this);
        this->senderError_->setStyleSheet(QStringLiteral("color: #d9534f;"));
        this->senderError_->setWordWrap(true);
        form->addRow(QString(), this->senderError_);
    }

    // Action
    this->actionEdit_ = new QLineEdit(existing.action, this);
    this->actionEdit_->setPlaceholderText(
        QStringLiteral("/ban {sender.name} spam"));
    form->addRow(QStringLiteral("Action"), this->actionEdit_);

    this->actionError_ = new QLabel(this);
    this->actionError_->setStyleSheet(QStringLiteral("color: #d9534f;"));
    this->actionError_->setWordWrap(true);
    form->addRow(QString(), this->actionError_);

    // Placeholder reference
    this->placeholderHelp_ = new QPlainTextEdit(this);
    this->placeholderHelp_->setReadOnly(true);
    this->placeholderHelp_->setMaximumHeight(56);
    this->placeholderHelp_->setPlainText(QStringLiteral(
        "Placeholders: {msg.id} {sender.name} {sender.displayName} "
        "{sender.id} {channel.name} {channel.id} {platform} "
        "(unknown placeholders expand to empty)."));
    form->addRow(QString(), this->placeholderHelp_);

    // Scope
    this->scopeCombo_ = new QComboBox(this);
    this->scopeCombo_->addItem(QStringLiteral("All except..."), kScopeAllExcept);
    this->scopeCombo_->addItem(QStringLiteral("Only in..."),    kScopeOnly);
    this->scopeCombo_->setCurrentIndex(
        existing.scope == LimerinoAutoAction::Scope::Only ? 1 : 0);
    form->addRow(QStringLiteral("Scope"), this->scopeCombo_);

    this->channelsEdit_ = new QLineEdit(existing.channels.join(
                                            QStringLiteral(", ")),
                                        this);
    this->channelsEdit_->setPlaceholderText(
        QStringLiteral("twitch:forsen, kick:amouranth"));
    form->addRow(QStringLiteral("Channel list"), this->channelsEdit_);

    // Cooldown
    this->cooldownSpin_ = new QSpinBox(this);
    this->cooldownSpin_->setRange(0, 3600);
    this->cooldownSpin_->setSuffix(QStringLiteral(" s"));
    this->cooldownSpin_->setValue(existing.cooldownSeconds);
    this->cooldownSpin_->setToolTip(
        QStringLiteral("0 = fire on every matching message."));
    form->addRow(QStringLiteral("Cooldown"), this->cooldownSpin_);

    this->addActionRow(root);

    // Test area
    auto *testLabel = new QLabel(QStringLiteral("Test (dry-run):"), this);
    root->addWidget(testLabel);

    auto *testForm = new QFormLayout();
    root->addLayout(testForm);

    this->testChannel_ = new QLineEdit(QStringLiteral("twitch:forsen"), this);
    this->testChannel_->setPlaceholderText(QStringLiteral("twitch:channel"));
    testForm->addRow(QStringLiteral("Channel"), this->testChannel_);

    this->testSender_ = new QLineEdit(this);
    this->testSender_->setPlaceholderText(QStringLiteral("someuser"));
    testForm->addRow(QStringLiteral("Sender"), this->testSender_);

    this->testMessage_ = new QPlainTextEdit(this);
    this->testMessage_->setPlaceholderText(QStringLiteral(
        "paste a sample message here"));
    this->testMessage_->setMaximumHeight(48);
    testForm->addRow(QStringLiteral("Message"), this->testMessage_);

    this->testResult_ = new QPlainTextEdit(this);
    this->testResult_->setReadOnly(true);
    this->testResult_->setMaximumHeight(70);
    testForm->addRow(QString(), this->testResult_);

    // Wire up validation changes.
    auto onChange = [this] {
        this->rebuildValidation();
    };
    QObject::connect(this->contentEdit_, &QLineEdit::textChanged, this, onChange);
    QObject::connect(this->senderEdit_, &QLineEdit::textChanged, this, onChange);
    QObject::connect(this->contentRegex_, &QCheckBox::toggled, this, onChange);
    QObject::connect(this->contentCase_, &QCheckBox::toggled, this, onChange);
    QObject::connect(this->senderRegex_, &QCheckBox::toggled, this, onChange);
    QObject::connect(this->senderCase_, &QCheckBox::toggled, this, onChange);
    QObject::connect(this->actionEdit_, &QLineEdit::textChanged, this, onChange);

    this->rebuildValidation();
}

void LimerinoAutoActionEditor::addActionRow(QVBoxLayout *layout)
{
    auto *row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);

    auto *testBtn = new QPushButton(QStringLiteral("Test"), this);
    row->addWidget(testBtn);
    row->addStretch(1);

    this->saveButton_ = new QPushButton(QStringLiteral("Save"), this);
    row->addWidget(this->saveButton_);

    layout->addLayout(row);

    QObject::connect(testBtn, &QPushButton::clicked, this, [this] {
        this->onTest();
    });
    QObject::connect(this->saveButton_, &QPushButton::clicked, this, [this] {
        this->onSave();
    });
}

void LimerinoAutoActionEditor::rebuildValidation()
{
    const LimerinoMatcher content(
        this->contentEdit_->text(), this->contentCase_->isChecked(),
        this->contentRegex_->isChecked());
    const LimerinoMatcher sender(
        this->senderEdit_->text(), this->senderCase_->isChecked(),
        this->senderRegex_->isChecked());

    if (auto err = content.compileError())
    {
        this->contentError_->setText(QStringLiteral("Invalid regex: ") + *err);
    }
    else
    {
        this->contentError_->clear();
    }
    if (auto err = sender.compileError())
    {
        this->senderError_->setText(QStringLiteral("Invalid regex: ") + *err);
    }
    else
    {
        this->senderError_->clear();
    }

    if (auto err = validateMatcherPair(content, sender))
    {
        this->actionError_->setText(*err);
    }
    else if (auto tplErr =
                 validateAutoActionTemplate(this->actionEdit_->text()))
    {
        this->actionError_->setText(*tplErr);
    }
    else
    {
        this->actionError_->clear();
    }
}

void LimerinoAutoActionEditor::onTest()
{
    const LimerinoMatcher content(
        this->contentEdit_->text(), this->contentCase_->isChecked(),
        this->contentRegex_->isChecked());
    const LimerinoMatcher sender(
        this->senderEdit_->text(), this->senderCase_->isChecked(),
        this->senderRegex_->isChecked());

    const QString channelKey = this->testChannel_->text().trimmed().toLower();
    const QString testSender = this->testSender_->text().trimmed();
    const QString testMsg = this->testMessage_->toPlainText();

    QStringList log;

    if (!channelKey.contains(QLatin1Char(':')))
    {
        log << QStringLiteral("Test channel must be platform:name (e.g. "
                              "\"twitch:forsen\").");
    }
    else if (content.compileError().has_value() ||
             sender.compileError().has_value())
    {
        log << QStringLiteral("Fix the regex errors above first.");
    }
    else if (validateMatcherPair(content, sender).has_value())
    {
        log << *validateMatcherPair(content, sender);
    }
    else
    {
        if (!content.matches(testMsg))
        {
            log << QStringLiteral("Content matcher does NOT match.");
        }
        if (!sender.matches(testSender))
        {
            log << QStringLiteral("Sender matcher does NOT match.");
        }

        AutoActionContext ctx;
        ctx.msgId = QStringLiteral("test-message-id");
        ctx.senderLogin = testSender.toLower();
        ctx.senderDisplayName = testSender;
        ctx.senderId = QStringLiteral("123456");
        ctx.channelName = channelKey.mid(channelKey.indexOf(QLatin1Char(':')) + 1);
        ctx.channelId = QStringLiteral("channelid");
        ctx.platform = channelKey.left(channelKey.indexOf(QLatin1Char(':')));

        const auto expanded =
            expandAutoAction(this->actionEdit_->text(), ctx, true);

        if (content.matches(testMsg) && sender.matches(testSender))
        {
            log << QStringLiteral("=> MATCHES. Action would be:");
            log << expanded.value_or(
                QStringLiteral("(skipped: placeholder value unavailable)"));
        }
        else
        {
            log << QStringLiteral("=> does NOT match.");
        }
    }

    this->testResult_->setPlainText(log.join(QLatin1Char('\n')));
}

void LimerinoAutoActionEditor::onSave()
{
    // Final hard validation: do not save if the matchers are broken.
    const LimerinoMatcher content(
        this->contentEdit_->text(), this->contentCase_->isChecked(),
        this->contentRegex_->isChecked());
    const LimerinoMatcher sender(
        this->senderEdit_->text(), this->senderCase_->isChecked(),
        this->senderRegex_->isChecked());

    if (auto err = validateMatcherPair(content, sender))
    {
        QMessageBox::warning(this, QStringLiteral("Cannot save"), *err);
        return;
    }
    if (content.compileError().has_value() || sender.compileError().has_value())
    {
        QMessageBox::warning(this, QStringLiteral("Cannot save"),
                             QStringLiteral("Fix the regex first."));
        return;
    }

    LimerinoAutoAction rule;
    rule.id = this->id_.isNull() ? QUuid::createUuid() : this->id_;
    rule.name = this->nameEdit_->text().trimmed();
    if (rule.name.isEmpty())
    {
        rule.name = QStringLiteral("Unnamed rule");
    }
    rule.enabled = this->enabledCheck_->isChecked();
    rule.content = content;
    rule.sender = sender;
    rule.action = this->actionEdit_->text().trimmed();
    rule.scope =
        this->scopeCombo_->currentIndex() == 1
            ? LimerinoAutoAction::Scope::Only
            : LimerinoAutoAction::Scope::AllExcept;
    rule.channels = this->channelsEdit_->text().split(
        QStringLiteral(","), Qt::SkipEmptyParts);
    for (auto &c : rule.channels)
    {
        c = c.trimmed().toLower();
    }
    rule.channels.removeAll({});
    rule.cooldownSeconds = this->cooldownSpin_->value();
    rule.normalize();

    emit ruleSaved(rule);
    this->close();
}

}  // namespace chatterino::limerino
