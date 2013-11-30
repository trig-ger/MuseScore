#include "importmidi_fraction.h"
#include "libmscore/mscore.h"

#include <limits>


namespace Ms {

//-----------------------------------------------------------------------------
// https://www.securecoding.cert.org/confluence/display/seccode/
// INT32-C.+Ensure+that+operations+on+signed+integers+do+not+result+in+overflow?showComments=false
//
// Access date: 2013.11.28

void checkAdditionOverflow(int a, int b)           // a + b
      {
      if ((b > 0 && a > (std::numeric_limits<int>::max() - b))
                  || (b < 0 && a < std::numeric_limits<int>::min() - b))
            {
            qDebug("ReducedFraction: addition overflow");
            abort();
            }
      }

void checkSubtractionOverflow(int a, int b)        // a - b
      {
      if ((b > 0 && a < std::numeric_limits<int>::min() + b)
                  || (b < 0 && a > std::numeric_limits<int>::max() + b))
            {
            qDebug("ReducedFraction: subtraction overflow");
            abort();
            }
      }

void checkMultiplicationOverflow(int a, int b)     // a * b
      {
      bool flag = false;

      if (a > 0) {
            if (b > 0) {
                  if (a > std::numeric_limits<int>::max() / b)
                        flag = true;
                  }
            else {
                  if (b < std::numeric_limits<int>::min() / a)
                        flag = true;
                  }
            }
      else {
            if (b > 0) {
                  if (a < std::numeric_limits<int>::min() / b)
                        flag = true;
                  }
            else {
                  if (a != 0 && b < std::numeric_limits<int>::max() / a)
                        flag = true;
                  }
            }

      if (flag) {
            qDebug("ReducedFraction: multiplication overflow");
            abort();
            }
      }

void checkDivisionOverflow(int a, int b)           // a / b, a % b
      {
      if ((b == 0) || ((a == std::numeric_limits<int>::min()) && (b == -1))) {
            qDebug("ReducedFraction: division overflow");
            abort();
            }
      }

void checkUnaryNegationOverflow(int a)             // -a
      {
      if (a == std::numeric_limits<int>::min()) {
            qDebug("ReducedFraction: unary nagation overflow");
            abort();
            }
      }

//-----------------------------------------------------------------------------

namespace {

// greatest common divisor

int gcd(int a, int b)
      {
      checkUnaryNegationOverflow(a);
      if (b == 0)
            return a < 0 ? -a : a;
      checkDivisionOverflow(a, b);
      return gcd(b, a % b);
      }

// least common multiple

unsigned lcm(int a, int b)
      {
      const int tmp = gcd(a, b);
      checkMultiplicationOverflow(a, b);
      checkDivisionOverflow(a * b, tmp);
      return a * b / tmp;
      }

} // namespace

//-----------------------------------------------------------------------------


ReducedFraction::ReducedFraction()
      : integral_(0)
      , numerator_(0)
      , denominator_(1)
      {
      }

ReducedFraction::ReducedFraction(int z, int n)
      : integral_(0)
      , numerator_(z)
      , denominator_(n)
      {
      extractIntegral();
      }

ReducedFraction::ReducedFraction(const Fraction &fraction)
      : integral_(0)
      , numerator_(fraction.numerator())
      , denominator_(fraction.denominator())
      {
      extractIntegral();
      }

Fraction ReducedFraction::fraction() const
      {
      return Fraction(numerator(), denominator_);
      }

int ReducedFraction::numerator() const
      {
      checkMultiplicationOverflow(integral_, denominator_);
      checkAdditionOverflow(integral_ * denominator_, numerator_);
      return integral_ * denominator_ + numerator_;
      }

ReducedFraction ReducedFraction::fromTicks(int ticks)
      {
      return ReducedFraction(ticks, MScore::division * 4);
      }

ReducedFraction ReducedFraction::reduced() const
      {
      const int tmp = gcd(numerator_, denominator_);
      checkDivisionOverflow(numerator_, tmp);
      checkDivisionOverflow(denominator_, tmp);
      return ReducedFraction(numerator_ / tmp, denominator_ / tmp);
      }

ReducedFraction ReducedFraction::absValue() const
      {
      auto tmp(*this);
      tmp.integral_ = qAbs(tmp.integral_);
      tmp.numerator_ = qAbs(tmp.numerator_);
      tmp.denominator_ = qAbs(tmp.denominator_);
      return tmp;
      }

int ReducedFraction::ticks() const
      {
      checkMultiplicationOverflow(numerator_, MScore::division * 4);
      checkAdditionOverflow(numerator_ * MScore::division * 4, denominator_ / 2);
      const int tmp = numerator_ * MScore::division * 4 + denominator_ / 2;
      checkDivisionOverflow(tmp, denominator_);
      checkMultiplicationOverflow(integral_, denominator_);

      return tmp / denominator_ + integral_ * MScore::division * 4;
      }

void ReducedFraction::reduce()
      {
      const int tmp = gcd(numerator_, denominator_);
      checkDivisionOverflow(numerator_, tmp);
      checkDivisionOverflow(denominator_, tmp);
      numerator_ /= tmp;
      denominator_ /= tmp;
      }

// helper function

int fractionPart(int lcmPart, int numerator, int denominator)
      {
      checkDivisionOverflow(lcmPart, denominator);
      const int part = lcmPart / denominator;
      checkMultiplicationOverflow(numerator, part);
      return numerator * part;
      }

ReducedFraction& ReducedFraction::operator+=(const ReducedFraction& val)
      {
      ReducedFraction value = val;        // input value: numerator < denominator always
      value.reduce();

      integral_ += value.integral_;
      const int tmp = lcm(denominator_, value.denominator_);
      numerator_ = fractionPart(tmp, numerator_, denominator_)
                  + fractionPart(tmp, value.numerator_, value.denominator_);
      denominator_ = tmp;
      extractIntegral();
      reduce();
      return *this;
      }

ReducedFraction& ReducedFraction::operator-=(const ReducedFraction& val)
      {
      ReducedFraction value = val;
      value.reduce();

      integral_ -= value.integral_;
      const int tmp = lcm(denominator_, value.denominator_);
      numerator_ = fractionPart(tmp, numerator_, denominator_)
                  - fractionPart(tmp, value.numerator_, value.denominator_);
      denominator_ = tmp;
      extractIntegral();
      reduce();
      return *this;
      }

ReducedFraction& ReducedFraction::operator*=(const ReducedFraction& val)
      {
      ReducedFraction value = val;
      value.reduce();

      checkMultiplicationOverflow(integral_, value.integral_);
      checkMultiplicationOverflow(value.integral_, numerator_);
      checkMultiplicationOverflow(integral_, value.numerator_);
      checkMultiplicationOverflow(numerator_, value.numerator_);
      checkMultiplicationOverflow(denominator_, value.denominator_);
                  // constructor automatically extracts integral part => less probability of overflow
      *this = ReducedFraction(integral_ * value.integral_, 1)
                  + ReducedFraction(value.integral_ * numerator_, denominator_)
                  + ReducedFraction(integral_ * value.numerator_, value.denominator_)
                  + ReducedFraction(numerator_ * value.numerator_, denominator_ * value.denominator_);
      reduce();
      return *this;
      }

ReducedFraction& ReducedFraction::operator*=(int val)
      {
      checkMultiplicationOverflow(integral_, val);
      checkMultiplicationOverflow(numerator_, val);
      integral_ *= val;
      numerator_ *= val;
      extractIntegral();
      reduce();
      return *this;
      }

ReducedFraction& ReducedFraction::operator/=(const ReducedFraction& val)
      {
      ReducedFraction value = val;
      value.reduce();

      checkMultiplicationOverflow(integral_, denominator_);
      checkAdditionOverflow(integral_ * denominator_, numerator_);
      checkMultiplicationOverflow(value.integral_, value.denominator_);
      checkAdditionOverflow(value.integral_ * value.denominator_, value.numerator_);

      *this = ReducedFraction(integral_ * denominator_ + numerator_,
                              value.integral_ * value.denominator_ + value.numerator_)
                  * ReducedFraction(value.denominator_, denominator_);
      reduce();
      return *this;
      }

ReducedFraction& ReducedFraction::operator/=(int val)
      {
      checkMultiplicationOverflow(denominator_, val);
      *this = ReducedFraction(integral_, val)
                  + ReducedFraction(numerator_, denominator_ * val);
      reduce();
      return *this;
      }

bool ReducedFraction::operator<(const ReducedFraction& val) const
      {
      const auto f1 = withPositiveNumerator(*this);
      const auto f2 = withPositiveNumerator(val);
      if (f1.integral_ != f2.integral_)
            return f1.integral_ < f2.integral_;
      const int v = lcm(f1.denominator_, f2.denominator_);
      return fractionPart(v, f1.numerator_, f1.denominator_)
                  < fractionPart(v, f2.numerator_, f2.denominator_);
      }

bool ReducedFraction::operator<=(const ReducedFraction& val) const
      {
      const auto f1 = withPositiveNumerator(*this);
      const auto f2 = withPositiveNumerator(val);
      if (f1.integral_ != f2.integral_)
            return f1.integral_ < val.integral_;
      const int v = lcm(f1.denominator_, f2.denominator_);
      return fractionPart(v, f1.numerator_, f1.denominator_)
                  <= fractionPart(v, f2.numerator_, f2.denominator_);
      }

bool ReducedFraction::operator>(const ReducedFraction& val) const
      {
      const auto f1 = withPositiveNumerator(*this);
      const auto f2 = withPositiveNumerator(val);
      if (f1.integral_ != f2.integral_)
            return f1.integral_ > f2.integral_;
      const int v = lcm(f1.denominator_, f2.denominator_);
      return fractionPart(v, f1.numerator_, f1.denominator_)
                  > fractionPart(v, f2.numerator_, f2.denominator_);
      }

bool ReducedFraction::operator>=(const ReducedFraction& val) const
      {
      const auto f1 = withPositiveNumerator(*this);
      const auto f2 = withPositiveNumerator(val);
      if (f1.integral_ != f2.integral_)
            return f1.integral_ > f2.integral_;
      const int v = lcm(f1.denominator_, f2.denominator_);
      return fractionPart(v, f1.numerator_, f1.denominator_)
                  >= fractionPart(v, f2.numerator_, f2.denominator_);
      }

bool ReducedFraction::operator==(const ReducedFraction& val) const
      {
      const auto f1 = withPositiveNumerator(*this);
      const auto f2 = withPositiveNumerator(val);
      if (f1.integral_ != f2.integral_)
            return false;
      const int v = lcm(f1.denominator_, f2.denominator_);
      return fractionPart(v, f1.numerator_, f1.denominator_)
                  == fractionPart(v, f2.numerator_, f2.denominator_);
      }

bool ReducedFraction::operator!=(const ReducedFraction& val) const
      {
      const auto f1 = withPositiveNumerator(*this);
      const auto f2 = withPositiveNumerator(val);
      if (f1.integral_ != f2.integral_)
            return true;
      const int v = lcm(f1.denominator_, f2.denominator_);
      return fractionPart(v, f1.numerator_, f1.denominator_)
                  != fractionPart(v, f2.numerator_, f2.denominator_);
      }

void ReducedFraction::extractIntegral()
      {
      checkDivisionOverflow(numerator_, denominator_);
      integral_ += numerator_ / denominator_;
      numerator_ %= denominator_;
//      if (numerator_ < 0) {
//            numerator_ = -qAbs(numerator_) % denominator_;
//            }
//      else
      //            numerator_ %= denominator_;
      }

ReducedFraction ReducedFraction::withPositiveNumerator(const ReducedFraction &f)
      {
      if (f.numerator_ >= 0)
            return f;
      ReducedFraction ff;
      ff.numerator_ = f.numerator_ + f.denominator_;
      ff.integral_ = f.integral_ - 1;
      ff.denominator_ = f.denominator_;
      return ff;
      }


//-------------------------------------------------------------------------

ReducedFraction toMuseScoreTicks(int tick, int oldDivision)
      {
      checkMultiplicationOverflow(tick, MScore::division);
      checkAdditionOverflow(tick * MScore::division, oldDivision / 2);
      const int tmp = tick * MScore::division + oldDivision / 2;
      checkDivisionOverflow(tmp, oldDivision);

      return ReducedFraction::fromTicks(tmp / oldDivision);
      }

} // namespace Ms
