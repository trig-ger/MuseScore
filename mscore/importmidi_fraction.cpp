#include "importmidi_fraction.h"
#include "libmscore/fraction.h"
#include "libmscore/mscore.h"

#include <limits>


namespace Ms {

//-----------------------------------------------------------------------------
// https://www.securecoding.cert.org/confluence/display/seccode/
// INT32-C.+Ensure+that+operations+on+signed+integers+do+not+result+in+overflow?showComments=false
//
// Access date: 2013.11.28

template<typename T>
void checkAdditionOverflow(T a, T b)           // a + b
      {
      if ((b > 0 && a > (std::numeric_limits<T>::max() - b))
                  || (b < 0 && a < std::numeric_limits<T>::min() - b))
            {
            qDebug("ReducedFraction: addition overflow");
            abort();
            }
      }

template<typename T>
void checkSubtractionOverflow(T a, T b)        // a - b
      {
      if ((b > 0 && a < std::numeric_limits<T>::min() + b)
                  || (b < 0 && a > std::numeric_limits<T>::max() + b))
            {
            qDebug("ReducedFraction: subtraction overflow");
            abort();
            }
      }

template<typename T>
void checkMultiplicationOverflow(T a, T b)     // a * b
      {
      bool flag = false;

      if (a > 0) {
            if (b > 0) {
                  if (a > std::numeric_limits<T>::max() / b)
                        flag = true;
                  }
            else {
                  if (b < std::numeric_limits<T>::min() / a)
                        flag = true;
                  }
            }
      else {
            if (b > 0) {
                  if (a < std::numeric_limits<T>::min() / b)
                        flag = true;
                  }
            else {
                  if (a != 0 && b < std::numeric_limits<T>::max() / a)
                        flag = true;
                  }
            }

      if (flag) {
            qDebug("ReducedFraction: multiplication overflow");
            abort();
            }
      }

template<typename T>
void checkDivisionOverflow(T a, T b)           // a / b
      {
      if ((b == 0) || ((a == std::numeric_limits<T>::min()) && (b == -1))) {
            qDebug("ReducedFraction: division overflow");
            abort();
            }
      }

template<typename T>
void checkRemainderOverflow(T a, T b)          // a % b
      {
      if ((b == 0) || ((a == std::numeric_limits<T>::min()) && (b == -1))) {
            qDebug("ReducedFraction: remainder overflow");
            abort();
            }
      }

template<typename T>
void checkUnaryNegationOverflow(T a)           // -a
      {
      if (a == std::numeric_limits<T>::min()) {
            qDebug("ReducedFraction: unary nagation overflow");
            abort();
            }
      }

template<typename T1, typename T2>
void checkCast(T1 from)
      {
      if (from > std::numeric_limits<T2>::max()
                  || from < std::numeric_limits<T2>::min()) {
            qDebug("ReducedFraction: type cast overflow");
            abort();
            }
      }

//-----------------------------------------------------------------------------

namespace {

// greatest common divisor

qint64 gcd(qint64 a, qint64 b)
      {
      checkUnaryNegationOverflow(a);
      if (b == 0)
            return a < 0 ? -a : a;
      checkRemainderOverflow(a, b);
      return gcd(b, a % b);
      }

// least common multiple

qint64 lcm(qint64 a, qint64 b)
      {
      const auto tmp = gcd(a, b);
      checkMultiplicationOverflow(a, b);
      checkDivisionOverflow(a * b, tmp);
      return a * b / tmp;
      }

} // namespace

//-----------------------------------------------------------------------------


ReducedFraction::ReducedFraction()
      : numerator_(0)
      , denominator_(1)
      {
      }

ReducedFraction::ReducedFraction(qint64 z, qint64 n)
      : numerator_(z)
      , denominator_(n)
      {
      }

ReducedFraction::ReducedFraction(const Fraction &fraction)
      : numerator_(fraction.numerator())
      , denominator_(fraction.denominator())
      {
      }

Fraction ReducedFraction::fraction() const
      {
      return Fraction(numerator_, denominator_);
      }

ReducedFraction ReducedFraction::fromTicks(int ticks)
      {
      return ReducedFraction(ticks, MScore::division * 4).reduced();
      }

ReducedFraction ReducedFraction::reduced() const
      {
      const auto tmp = gcd(numerator_, denominator_);
      checkDivisionOverflow(numerator_, tmp);
      checkDivisionOverflow(denominator_, tmp);
      return ReducedFraction(numerator_ / tmp, denominator_ / tmp);
      }

ReducedFraction ReducedFraction::absValue() const
      {
      return ReducedFraction(qAbs(numerator_), qAbs(denominator_));
      }

int ReducedFraction::ticks() const
      {
      checkMultiplicationOverflow(numerator_, static_cast<qint64>(MScore::division * 4));
      checkAdditionOverflow(numerator_ * MScore::division * 4, denominator_ / 2);
      const auto tmp = numerator_ * MScore::division * 4 + denominator_ / 2;
      checkDivisionOverflow(tmp, denominator_);
      const auto result = tmp / denominator_;
      checkCast<qint64, int>(result);

      return static_cast<int>(result);
      }

void ReducedFraction::reduce()
      {
      const auto tmp = gcd(numerator_, denominator_);
      checkDivisionOverflow(numerator_, tmp);
      checkDivisionOverflow(denominator_, tmp);
      numerator_ /= tmp;
      denominator_ /= tmp;
      }

// helper function

qint64 fractionPart(qint64 lcmPart, qint64 numerator, qint64 denominator)
      {
      checkDivisionOverflow(lcmPart, denominator);
      const auto part = lcmPart / denominator;
      checkMultiplicationOverflow(numerator, part);
      return part;
      }

ReducedFraction& ReducedFraction::operator+=(const ReducedFraction& val)
      {
      reduce();
      ReducedFraction value = val;
      value.reduce();

      const auto tmp = lcm(denominator_, val.denominator_);
      numerator_ = numerator_ * fractionPart(tmp, numerator_, denominator_)
                  + val.numerator_ * fractionPart(tmp, val.numerator_, val.denominator_);
      denominator_ = tmp;
      return *this;
      }

ReducedFraction& ReducedFraction::operator-=(const ReducedFraction& val)
      {
      reduce();
      ReducedFraction value = val;
      value.reduce();

      const auto tmp = lcm(denominator_, val.denominator_);
      numerator_ = numerator_ * fractionPart(tmp, numerator_, denominator_)
                  - val.numerator_ * fractionPart(tmp, val.numerator_, val.denominator_);
      denominator_ = tmp;
      return *this;
      }

ReducedFraction& ReducedFraction::operator*=(const ReducedFraction& val)
      {
      reduce();
      ReducedFraction value = val;
      value.reduce();

      checkMultiplicationOverflow(numerator_, val.numerator_);
      checkMultiplicationOverflow(denominator_, val.denominator_);
      numerator_ *= val.numerator_;
      denominator_  *= val.denominator_;
      return *this;
      }

ReducedFraction& ReducedFraction::operator*=(qint64 val)
      {
      reduce();
      checkMultiplicationOverflow(numerator_, val);
      numerator_ *= val;
      return *this;
      }

ReducedFraction& ReducedFraction::operator/=(const ReducedFraction& val)
      {
      reduce();
      ReducedFraction value = val;
      value.reduce();

      checkMultiplicationOverflow(numerator_, val.denominator_);
      checkMultiplicationOverflow(denominator_, val.numerator_);
      numerator_ *= val.denominator_;
      denominator_  *= val.numerator_;
      return *this;
      }

ReducedFraction& ReducedFraction::operator/=(qint64 val)
      {
      reduce();
      checkMultiplicationOverflow(denominator_, val);
      denominator_ *= val;
      return *this;
      }

bool ReducedFraction::operator<(const ReducedFraction& val) const
      {
      const auto v = lcm(denominator_, val.denominator_);
      return numerator_ * fractionPart(v, numerator_, denominator_)
                  < val.numerator_ * fractionPart(v, val.numerator_, val.denominator_);
      }

bool ReducedFraction::operator<=(const ReducedFraction& val) const
      {
      const auto v = lcm(denominator_, val.denominator_);
      return numerator_ * fractionPart(v, numerator_, denominator_)
                  <= val.numerator_ * fractionPart(v, val.numerator_, val.denominator_);
      }

bool ReducedFraction::operator>(const ReducedFraction& val) const
      {
      const auto v = lcm(denominator_, val.denominator_);
      return numerator_ * fractionPart(v, numerator_, denominator_)
                  > val.numerator_ * fractionPart(v, val.numerator_, val.denominator_);
      }

bool ReducedFraction::operator>=(const ReducedFraction& val) const
      {
      const auto v = lcm(denominator_, val.denominator_);
      return numerator_ * fractionPart(v, numerator_, denominator_)
                  >= val.numerator_ * fractionPart(v, val.numerator_, val.denominator_);
      }

bool ReducedFraction::operator==(const ReducedFraction& val) const
      {
      const auto v = lcm(denominator_, val.denominator_);
      return numerator_ * fractionPart(v, numerator_, denominator_)
                  == val.numerator_ * fractionPart(v, val.numerator_, val.denominator_);
      }

bool ReducedFraction::operator!=(const ReducedFraction& val) const
      {
      const auto v = lcm(denominator_, val.denominator_);
      return numerator_ * fractionPart(v, numerator_, denominator_)
                  != val.numerator_ * fractionPart(v, val.numerator_, val.denominator_);
      }


//-------------------------------------------------------------------------

ReducedFraction toMuseScoreTicks(int tick, int oldDivision)
      {
      const auto bigTick = static_cast<qint64>(tick);
      const auto bigDiv = static_cast<qint64>(oldDivision);

      checkMultiplicationOverflow(bigTick, static_cast<qint64>(MScore::division));
      checkAdditionOverflow(bigTick * MScore::division, bigDiv / 2);
      const auto tmp = bigTick * MScore::division + bigDiv / 2;
      checkDivisionOverflow(tmp, bigDiv);
      const auto result = tmp / bigDiv;
      checkCast<qint64, int>(result);

      return ReducedFraction::fromTicks(static_cast<int>(result));
      }

} // namespace Ms
