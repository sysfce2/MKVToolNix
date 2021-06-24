#include "common/common_pch.h"

#include <QDateTimeEdit>

#include "common/qt.h"
#include "mkvtoolnix-gui/header_editor/time_value_page.h"
#include "mkvtoolnix-gui/main_window/main_window.h"
#include "mkvtoolnix-gui/util/date_time.h"
#include "mkvtoolnix-gui/util/settings.h"

namespace mtx::gui::HeaderEditor {

using namespace mtx::gui;

TimeValuePage::TimeValuePage(Tab &parent,
                             PageBase &topLevelPage,
                             EbmlMaster &master,
                             EbmlCallbacks const &callbacks,
                             translatable_string_c const &title,
                             translatable_string_c const &description)
  : ValuePage{parent, topLevelPage, master, callbacks, ValueType::Timestamp, title, description}
{
  connect(MainWindow::get(), &MainWindow::preferencesChanged, this, &TimeValuePage::showInRequestedTimeSpec);
}

TimeValuePage::~TimeValuePage() {
}

QWidget *
TimeValuePage::createInputControl() {
  auto &cfg = Util::Settings::get();

  if (m_element)
    m_originalValueUTC = QDateTime::fromMSecsSinceEpoch(static_cast<EbmlDate *>(m_element)->GetEpochDate() * 1000, Qt::UTC);

  m_dteValue = new QDateTimeEdit{this};
  m_dteValue->setCalendarPopup(true);
  m_dteValue->setTimeSpec(cfg.m_headerEditorDateTimeInUTC ? Qt::UTC            : Qt::LocalTime);
  m_dteValue->setDateTime(cfg.m_headerEditorDateTimeInUTC ? m_originalValueUTC : m_originalValueUTC.toLocalTime());
  m_dteValue->setDisplayFormat(Q("yyyy-MM-dd hh:mm:ss"));

  return m_dteValue;
}

QString
TimeValuePage::originalValueAsString()
  const {
  auto &cfg = Util::Settings::get();

  return Util::displayableDate(cfg.m_headerEditorDateTimeInUTC ? m_originalValueUTC : m_originalValueUTC.toLocalTime());
}

QString
TimeValuePage::currentValueAsString()
  const {
  return Util::displayableDate(m_dteValue->dateTime());
}

void
TimeValuePage::resetValue() {
  auto &cfg = Util::Settings::get();

  m_dteValue->setDateTime(cfg.m_headerEditorDateTimeInUTC ? m_originalValueUTC : m_originalValueUTC.toLocalTime());
}

bool
TimeValuePage::validateValue()
  const {
  return m_dteValue->dateTime().isValid();
}

void
TimeValuePage::copyValueToElement() {
  static_cast<EbmlDate *>(m_element)->SetEpochDate(m_dteValue->dateTime().toUTC().toMSecsSinceEpoch() / 1000);
}

void
TimeValuePage::showInRequestedTimeSpec() {
  auto &cfg    = Util::Settings::get();
  auto current = m_dteValue->dateTime();

  m_dteValue->setTimeSpec(cfg.m_headerEditorDateTimeInUTC ? Qt::UTC : Qt::LocalTime);
  m_dteValue->setDateTime(cfg.m_headerEditorDateTimeInUTC ? current.toUTC() : current.toLocalTime());
}

QString
TimeValuePage::note()
  const {
  auto &cfg = Util::Settings::get();
  auto now  = QDateTime::currentDateTime();

  return cfg.m_headerEditorDateTimeInUTC || !now.offsetFromUtc() ? QY("The date & time shown is in UTC.") : QY("The date & time shown is in your local time zone.");
}

}
