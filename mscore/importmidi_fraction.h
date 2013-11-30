#ifndef IMPORTMIDI_FRACTION_H
#define IMPORTMIDI_FRACTION_H


namespace Ms {

class Fraction;

class ReducedFraction
      {
   public:
      ReducedFraction();
      ReducedFraction(qint64 z, qint64 n);
      explicit ReducedFraction(const Fraction &);

      Fraction fraction() const;
      qint64 numerator() const { return numerator_; }
      qint64 denominator() const { return denominator_; }

      static ReducedFraction fromTicks(int ticks);
      ReducedFraction reduced() const;
      ReducedFraction absValue() const;
      int ticks() const;
      void reduce();

      ReducedFraction& operator+=(const ReducedFraction&);
      ReducedFraction& operator-=(const ReducedFraction&);
      ReducedFraction& operator*=(const ReducedFraction&);
      ReducedFraction& operator*=(qint64);
      ReducedFraction& operator/=(const ReducedFraction&);
      ReducedFraction& operator/=(qint64);

      ReducedFraction operator+(const ReducedFraction& v) const { return ReducedFraction(*this) += v; }
      ReducedFraction operator-(const ReducedFraction& v) const { return ReducedFraction(*this) -= v; }
      ReducedFraction operator*(const ReducedFraction& v) const { return ReducedFraction(*this) *= v; }
      ReducedFraction operator*(qint64 v)                 const { return ReducedFraction(*this) *= v; }
      ReducedFraction operator/(const ReducedFraction& v) const { return ReducedFraction(*this) /= v; }
      ReducedFraction operator/(qint64 v)                 const { return ReducedFraction(*this) /= v; }

      bool operator<(const ReducedFraction&) const;
      bool operator<=(const ReducedFraction&) const;
      bool operator>=(const ReducedFraction&) const;
      bool operator>(const ReducedFraction&) const;
      bool operator==(const ReducedFraction&) const;
      bool operator!=(const ReducedFraction&) const;

   private:
      qint64 numerator_;
      qint64 denominator_;
      };

ReducedFraction toMuseScoreTicks(int tick, int oldDivision);

} // namespace Ms


#endif // IMPORTMIDI_FRACTION_H
