/*
 *      Copyright (C) 2012-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "ServiceBroker.h"
#include "dialogs/GUIDialogOK.h"
#include "dialogs/GUIDialogProgress.h"
#include "epg/EpgContainer.h"
#include "guilib/GUIWindowManager.h"
#include "input/Key.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"

#include "pvr/PVRGUIActions.h"
#include "pvr/PVRItem.h"
#include "pvr/PVRManager.h"
#include "pvr/addons/PVRClients.h"
#include "pvr/channels/PVRChannelGroupsContainer.h"
#include "pvr/dialogs/GUIDialogPVRGuideSearch.h"

#include "GUIWindowPVRSearch.h"

using namespace PVR;
using namespace EPG;

CGUIWindowPVRSearch::CGUIWindowPVRSearch(bool bRadio) :
  CGUIWindowPVRBase(bRadio, bRadio ? WINDOW_RADIO_SEARCH : WINDOW_TV_SEARCH, "MyPVRSearch.xml"),
  m_bSearchConfirmed(false)
{
}

void CGUIWindowPVRSearch::GetContextButtons(int itemNumber, CContextButtons &buttons)
{
  if (itemNumber < 0 || itemNumber >= m_vecItems->Size())
    return;

  buttons.Add(CONTEXT_BUTTON_CLEAR, 19232);               /* Clear search results */

  CEpgInfoTagPtr epg(pItem->GetEPGInfoTag());
  if (epg)
  {
    buttons.Add(CONTEXT_BUTTON_INFO, 19047);              /* Programme information */

    CPVRTimerInfoTagPtr timer(epg->Timer());
    if (timer)
    {
      if (timer->GetTimerRuleId() != PVR_TIMER_NO_PARENT)
      {
        buttons.Add(CONTEXT_BUTTON_EDIT_TIMER_RULE, 19243); /* Edit timer rule */
        buttons.Add(CONTEXT_BUTTON_DELETE_TIMER_RULE, 19295); /* Delete timer rule */
      }

      const CPVRTimerTypePtr timerType(timer->GetTimerType());
      if (timerType && !timerType->IsReadOnly() && timer->GetTimerRuleId() == PVR_TIMER_NO_PARENT)
        buttons.Add(CONTEXT_BUTTON_EDIT_TIMER, 19242);    /* Edit timer */
      else
        buttons.Add(CONTEXT_BUTTON_EDIT_TIMER, 19241);    /* View timer information */

      if (timer->IsRecording())
        buttons.Add(CONTEXT_BUTTON_STOP_RECORD, 19059);   /* Stop recording */
      else
      {
        if (timerType && !timerType->IsReadOnly())
          buttons.Add(CONTEXT_BUTTON_DELETE_TIMER, 19060);  /* Delete timer */
      }
    }
    else if (g_PVRClients->SupportsTimers())
    {
      if (epg->IsRecordable())
        buttons.Add(CONTEXT_BUTTON_START_RECORD, 264);      /* Record */
      buttons.Add(CONTEXT_BUTTON_ADD_TIMER, 19061);       /* Add timer */
    }

    if (epg->HasRecording())
      buttons.Add(CONTEXT_BUTTON_PLAY_ITEM, 19687);       /* Play recording */

    CPVRChannelPtr channel(epg->ChannelTag());
    if (channel &&
        g_PVRClients->HasMenuHooks(channel->ClientID(), PVR_MENUHOOK_EPG))
      buttons.Add(CONTEXT_BUTTON_MENU_HOOKS, 19195);      /* PVR client specific action */
  }

  CGUIWindowPVRBase::GetContextButtons(itemNumber, buttons);
}

bool CGUIWindowPVRSearch::OnContextButton(int itemNumber, CONTEXT_BUTTON button)
{
  if (itemNumber < 0 || itemNumber >= m_vecItems->Size())
    return false;
  CFileItemPtr pItem = m_vecItems->Get(itemNumber);

  return OnContextButtonClear(pItem.get(), button) ||
      CGUIMediaWindow::OnContextButton(itemNumber, button);
}

void CGUIWindowPVRSearch::SetItemToSearch(const CFileItemPtr &item)
{
  m_searchfilter.Reset();

  if (item->IsUsablePVRRecording())
  {
    m_searchfilter.SetSearchPhrase(item->GetPVRRecordingInfoTag()->m_strTitle);
  }
  else
  {
    const CEpgInfoTagPtr epgTag(CPVRItem(item).GetEpgInfoTag());
    if (epgTag)
      m_searchfilter.SetSearchPhrase(epgTag->Title());
  }

  m_bSearchConfirmed = true;

  if (IsActive())
    Refresh(true);
}

void CGUIWindowPVRSearch::OnPrepareFileItems(CFileItemList &items)
{
  bool bAddSpecialSearchItem = items.IsEmpty();

  if (m_bSearchConfirmed)
  {
    bAddSpecialSearchItem = true;

    items.Clear();
    CGUIDialogProgress* dlgProgress = g_windowManager.GetWindow<CGUIDialogProgress>();
    if (dlgProgress)
    {
      dlgProgress->SetHeading(CVariant{194}); // "Searching..."
      dlgProgress->SetText(CVariant{m_searchfilter.GetSearchTerm()});
      dlgProgress->Open();
      dlgProgress->Progress();
    }

    //! @todo should we limit the find similar search to the selected group?
    g_EpgContainer.GetEPGSearch(items, m_searchfilter);

    if (dlgProgress)
      dlgProgress->Close();

    if (items.IsEmpty())
      CGUIDialogOK::ShowAndGetInput(CVariant{194},  // "Searching..."
                                    CVariant{284}); // "No results found"
  }

  if (bAddSpecialSearchItem)
  {
    CFileItemPtr item(new CFileItem("pvr://guide/searchresults/search/", true));
    item->SetLabel(g_localizeStrings.Get(19140)); // "Search..."
    item->SetLabelPreformatted(true);
    item->SetSpecialSort(SortSpecialOnTop);
    items.Add(item);
  }
}

bool CGUIWindowPVRSearch::OnMessage(CGUIMessage &message)
{
  if (message.GetMessage() == GUI_MSG_CLICKED)
  {
    if (message.GetSenderId() == m_viewControl.GetCurrentControl())
    {
      int iItem = m_viewControl.GetSelectedItem();
      if (iItem >= 0 && iItem < m_vecItems->Size())
      {
        CFileItemPtr pItem = m_vecItems->Get(iItem);

        /* process actions */
        switch (message.GetParam1())
        {
          case ACTION_SHOW_INFO:
          case ACTION_SELECT_ITEM:
          case ACTION_MOUSE_LEFT_CLICK:
          {
            if (URIUtils::PathEquals(pItem->GetPath(), "pvr://guide/searchresults/search/"))
              OpenDialogSearch();
            else
               CServiceBroker::GetPVRManager().GUIActions()->ShowEPGInfo(pItem);
            return true;
          }

          case ACTION_CONTEXT_MENU:
          case ACTION_MOUSE_RIGHT_CLICK:
            OnPopupMenu(iItem);
            return true;

          case ACTION_RECORD:
            CServiceBroker::GetPVRManager().GUIActions()->ToggleTimer(pItem);
            return true;
        }
      }
    }
  }

  return CGUIWindowPVRBase::OnMessage(message);
}

bool CGUIWindowPVRSearch::OnContextButtonClear(CFileItem *item, CONTEXT_BUTTON button)
{
  bool bReturn = false;

  if (button == CONTEXT_BUTTON_CLEAR)
  {
    bReturn = true;

    m_bSearchConfirmed = false;
    m_searchfilter.Reset();

    Refresh(true);
  }

  return bReturn;
}

void CGUIWindowPVRSearch::OpenDialogSearch()
{
  CGUIDialogPVRGuideSearch* dlgSearch = g_windowManager.GetWindow<CGUIDialogPVRGuideSearch>();

  if (!dlgSearch)
    return;

  dlgSearch->SetFilterData(&m_searchfilter);

  /* Set channel type filter */
  m_searchfilter.SetIsRadio(m_bRadio);

  /* Open dialog window */
  dlgSearch->Open();

  if (dlgSearch->IsConfirmed())
  {
    m_bSearchConfirmed = true;
    Refresh(true);
  }
}
