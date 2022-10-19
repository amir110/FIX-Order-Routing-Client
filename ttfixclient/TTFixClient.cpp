/*****************************************************************

THIS SOFTWARE IS OPEN SOURCE UNDER THE MIT LICENSE
SUPPORT IS AVAILABE FROM THE AUTHORS

Copyright 2022 Vincent Maciejewski, Quant Enterprises & M2 Tech
Contact:
v@m2te.ch
mayeski@gmail.com
https://www.linkedin.com/in/vmayeski/
http://m2te.ch/


Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

https://opensource.org/licenses/MIT

*****************************************************************/

#include <iostream>
#include "TTFixClient.hpp"

using namespace std;
using namespace m2::ttfix;

void TTFixClient::toAdmin(FIX::Message &m, const FIX::SessionID &)
{
  const auto &msgTypeValue = m.getHeader().getField(FIX::FIELD::MsgType);
  if (msgTypeValue == "A")
  {
    // this is a logon message
    m.setField(FIX::FIELD::Password, password);
  }
}

void TTFixClient::fromAdmin(const FIX::Message &, const FIX::SessionID &)
{
}

void TTFixClient::onLogon(const FIX::SessionID &)
{
  cb->onLogon();
  isLoggedOn = true;
}

void TTFixClient::onLogout(const FIX::SessionID &)
{
  cb->onLogout();
  isLoggedOn = false;
}

// message from TT can be intercepted here
void TTFixClient::fromApp(const FIX::Message &message, const FIX::SessionID &sessionID)
{
  // process message
  crack(message, sessionID);
}

// message to TT can be intercepted here
void TTFixClient::toApp(FIX::Message &message, const FIX::SessionID &)
{
  // https://www.onixs.biz/fix-dictionary/FIXT1.1/tagNum_43.html
  // check for duplicate transmission to TT
  FIX::PossDupFlag possDupFlag = false;
  message.getHeader().getFieldIfSet(possDupFlag);
  if (possDupFlag)
    throw FIX::DoNotSend();

  // do not send rejects to TT
  // (these are often caused by missing field exceptions in onMessage callbacks)
  const auto &msgTypeValue = message.getHeader().getField(FIX::FIELD::MsgType);
  if (msgTypeValue == "j")
  {
    std::cout << "*** attempt to send reject to TT ***\n";
    std::cerr << "*** attempt to send reject to TT ***\n";
    throw FIX::DoNotSend();
  }
}

void TTFixClient::onMessage(const FIX44::ExecutionReport &m, const FIX::SessionID &)
{
  const auto &status_str = m.getField(FIX::FIELD::OrdStatus);
  const auto &status = convertOrdStatus(status_str);
  const auto &exec_typ_str = m.getField(FIX::FIELD::ExecType);
  const auto &exec_typ = convertExecType(exec_typ_str);
  const auto &cloid_ = m.getField(FIX::FIELD::ClOrdID);
  const auto &cloid = stoull(cloid_);
  const auto &ttoid = m.getField(FIX::FIELD::OrderID);
  const auto &cmeoid_ = m.getField(FIX::FIELD::SecondaryOrderID);
  const auto &cmeoid = stoull(cmeoid_);
  const auto &qty = stoi(m.getField(FIX::FIELD::OrderQty));
  const auto &cum_qty = stoi(m.getField(FIX::FIELD::CumQty));
  const auto &leaves_qty = stoi(m.getField(FIX::FIELD::LeavesQty));
  const auto &txtime = m.getField(FIX::FIELD::TransactTime);
  const auto &TTrecvtime = m.getField(16561);
  const auto &sendingTime = m.getHeader().getField(FIX::FIELD::SendingTime);
  string &&px_str="";
  double px = 0;
  bool has_px = false;
  if (m.isSetField(FIX::FIELD::LastPx))
  {
    px_str = m.getField(FIX::FIELD::LastPx);
    px = stod(px_str);
    has_px = true;
  }
  std::string &&text = "";
  if (m.isSetField(FIX::FIELD::Text))
    text = m.getField(FIX::FIELD::Text);
  cb->onExecutionReport(
      status,
      exec_typ,
      cloid,
      ttoid,
      cmeoid,
      qty,
      cum_qty,
      leaves_qty,
      has_px,
      px,
      txtime,
      TTrecvtime,
      sendingTime,
      text);
}

void TTFixClient::onMessage(const FIX44::OrderCancelReject &m, const FIX::SessionID &)
{
  const auto &clOrdID_str = m.getField(FIX::FIELD::ClOrdID);
  const auto &clOrdId = stoull(clOrdID_str);
  const auto &origClOrdId_str = m.getField(FIX::FIELD::OrigClOrdID);
  const auto &origClOrdId = stoull(origClOrdId_str);
  const auto &ttoid = m.getField(FIX::FIELD::OrderID);
  string &&cmeoid_ = "";
  uint64_t cmeoid = 0;
  if (m.isSetField(FIX::FIELD::SecondaryOrderID))
  {
    cmeoid_ = m.getField(FIX::FIELD::SecondaryOrderID);
    cmeoid = stoull(cmeoid_);
  }
  const auto &status_str = m.getField(FIX::FIELD::OrdStatus);
  const auto &status = convertOrdStatus(status_str);
  const auto &cxlRejResponseTo_str = m.getField(FIX::FIELD::CxlRejResponseTo);
  const auto &cxlRejResponseTo = stoi(cxlRejResponseTo_str);
  const auto &cxlRejReason_str = m.getField(FIX::FIELD::CxlRejReason);
  const auto &cxlRejReason = stoi(cxlRejReason_str);
  string &&complianceText = "";
  if (m.isSetField(2404))
    complianceText = m.getField(2404); // ComplianceText
  const auto &txtime = m.getField(FIX::FIELD::TransactTime);
  string &&TTrecvtime = "";
  if (m.isSetField(16561))
    TTrecvtime = m.getField(16561); // TimeReceivedFromExchange
  std::string &&text = "";
  if (m.isSetField(FIX::FIELD::Text))
    text = m.getField(FIX::FIELD::Text);
  cb->onOrderCancelReject(
      status,
      clOrdId,
      origClOrdId,
      ttoid,
      cmeoid,
      cxlRejResponseTo,
      cxlRejReason,
      complianceText,
      txtime,
      TTrecvtime,
      text);
}

// session level reject
// NOTE: this function was not tested
void TTFixClient::onMessage(const FIX44::Reject &m, const FIX::SessionID &)
{
  const auto &rejReason_str = m.getField(FIX::FIELD::SessionRejectReason);
  const auto &rejReason = std::stoi(rejReason_str);
  const auto &rejMsgTyp = m.getField(FIX::FIELD::RefMsgType);
  const auto &text = m.getField(FIX::FIELD::Text);
  const auto &refSeqNum_str = m.getField(FIX::FIELD::RefSeqNum);
  const auto &refSeqNum = stoi(refSeqNum_str);
  cb->onSessionReject(rejReason, rejMsgTyp, text, refSeqNum);
}

void TTFixClient::onMessage(const FIX44::BusinessMessageReject &m, const FIX::SessionID &)
{
  const auto &rejReason_str = m.getField(FIX::FIELD::BusinessRejectReason);
  const auto &rejReason = stoi(rejReason_str);
  const auto &text = m.getField(FIX::FIELD::Text);
  const auto &rejMsgTyp = m.getField(FIX::FIELD::RefMsgType);
  const auto &refSeqNum_str = m.getField(FIX::FIELD::RefSeqNum);
  const auto &refSeqNum = stoi(refSeqNum_str);
  cb->onBusinessReject(rejReason, rejMsgTyp, text, refSeqNum);
}

void TTFixClient::onMessage(const FIX44::News &m, const FIX::SessionID &)
{
  const auto &text = m.getField(FIX::FIELD::Text);
  noNews = false;
  cb->onNews(text);
}

uint64_t TTFixClient::sendNewOrderSingle(
    const char *cficode,
    const char *securityId,
    const char *symbol,
    double price,
    int qty,
    char bs,
    const char *orderType,
    const char *timeInForce)
{

  if (noNews)
    throw runtime_error("must wait for news message");

  if (!isLoggedOn)
    throw runtime_error("not logged on");

  const auto &ret = createCLOID();
  FIX::ClOrdID oid = get<1>(ret);

  FIX::Side side;
  if (bs == 'B')
    side = FIX::Side_BUY;
  else if (bs == 'S')
    side = FIX::Side_SELL;
  else
    throw runtime_error("invalid side");

  FIX::OrdType ordType;
  if (!strcmp(orderType, "MKT"))
    ordType = FIX::OrdType_MARKET;
  else if (!strcmp(orderType, "LIM"))
    ordType = FIX::OrdType_LIMIT;
  else if (!strcmp(orderType, "STP"))
    ordType = FIX::OrdType_STOP;
  else if (!strcmp(orderType, "STL"))
    ordType = FIX::OrdType_STOP_LIMIT;
  else
    throw runtime_error("invalid order type");

  FIX44::NewOrderSingle newOrderSingle(
      oid,
      side,
      FIX::TransactTime(),
      ordType);

  FIX::TimeInForce tif;
  if (!strcmp(timeInForce, "DAY"))
    tif = FIX::TimeInForce_DAY;
  else if (!strcmp(timeInForce, "IOC"))
    tif = FIX::TimeInForce_IMMEDIATE_OR_CANCEL;
  else if (!strcmp(timeInForce, "OPG"))
    tif = FIX::TimeInForce_AT_THE_OPENING;
  else if (!strcmp(timeInForce, "GTC"))
    tif = FIX::TimeInForce_GOOD_TILL_CANCEL;
  else if (!strcmp(timeInForce, "GTX"))
    tif = FIX::TimeInForce_GOOD_TILL_CROSSING;
  else
    throw runtime_error("invalid time in force");
  newOrderSingle.set(tif);

  if (ordType == FIX::OrdType_LIMIT || ordType == FIX::OrdType_STOP_LIMIT)
    newOrderSingle.set(FIX::Price(price));
  if (ordType == FIX::OrdType_STOP || ordType == FIX::OrdType_STOP_LIMIT)
    newOrderSingle.set(FIX::StopPx(price));
  newOrderSingle.set(FIX::OrderQty(qty));
  newOrderSingle.set(FIX::Account(account));
  newOrderSingle.set(FIX::SecurityExchange(exchange));
  newOrderSingle.set(FIX::CFICode(cficode));
  newOrderSingle.set(FIX::SecurityID(securityId));
  newOrderSingle.set(FIX::Symbol(symbol));
  newOrderSingle.set(FIX::SecurityIDSource("98")); // name
  newOrderSingle.getHeader().setField(FIX::SenderCompID(senderCompId));
  newOrderSingle.getHeader().setField(FIX::TargetCompID(targetCompId));

  FIX::Session::sendToTarget(newOrderSingle);

  ordInfo.emplace(
      make_pair(
          get<0>(ret),
          OrdInfo(
              side,
              price,
              ordType,
              qty)));

  return get<0>(ret);
}

uint64_t TTFixClient::sendCancelOrderRequest(
    uint64_t origClOrdId)
{

  if (noNews)
    throw runtime_error("must wait for news message");

  if (!isLoggedOn)
    throw runtime_error("not logged on");

  // looking up the order because side is a required field
  auto p = ordInfo.find(origClOrdId);
  if (p == ordInfo.end())
    throw runtime_error("orig client id not found");

  const auto &oid = createCLOID();

  // constructor takes required fields
  FIX44::OrderCancelRequest orderCancelRequest(
      FIX::OrigClOrdID(std::to_string(origClOrdId)),
      FIX::ClOrdID(get<1>(oid)),
      p->second.side,
      FIX::TransactTime());

  orderCancelRequest.set(account);
  orderCancelRequest.getHeader().setField(senderCompId);
  orderCancelRequest.getHeader().setField(targetCompId);

  FIX::Session::sendToTarget(orderCancelRequest);

  return get<0>(oid);
}

uint64_t TTFixClient::sendCancelReplaceRequest(
    uint64_t origClOrdId,
    int qty,
    double price,
    bool has_price)
{

  if (noNews)
    throw runtime_error("must wait for news message");

  if (!isLoggedOn)
    throw runtime_error("not logged on");

  auto p = ordInfo.find(origClOrdId);
  if (p == ordInfo.end())
    throw runtime_error("orig client id not found");

  const auto &oid = createCLOID();

  // constructor takes required fields
  FIX44::OrderCancelReplaceRequest cancelReplaceRequest(
      FIX::OrigClOrdID(std::to_string(origClOrdId)),
      FIX::ClOrdID(get<1>(oid)),
      p->second.side,
      FIX::TransactTime(),
      p->second.ordType);

  double price_to_repl;
  if (has_price)
    price_to_repl = price;
  else
    price_to_repl = p->second.price;
  if (p->second.ordType == FIX::OrdType_LIMIT || p->second.ordType == FIX::OrdType_STOP_LIMIT)
    cancelReplaceRequest.set(FIX::Price(price_to_repl));
  if (p->second.ordType == FIX::OrdType_STOP || p->second.ordType == FIX::OrdType_STOP_LIMIT)
    cancelReplaceRequest.set(FIX::StopPx(price_to_repl));
  if (qty > 0)
    cancelReplaceRequest.set(FIX::OrderQty(qty));
  else
    cancelReplaceRequest.set(FIX::OrderQty(p->second.qty_remaining));
  cancelReplaceRequest.set(account);
  cancelReplaceRequest.getHeader().setField(senderCompId);
  cancelReplaceRequest.getHeader().setField(targetCompId);

  FIX::Session::sendToTarget(cancelReplaceRequest);

  return get<0>(oid);
}