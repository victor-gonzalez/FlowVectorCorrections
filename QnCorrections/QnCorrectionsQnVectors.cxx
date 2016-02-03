/**************************************************************************************************
 *                                                                                                *
 * Package:       FlowVectorCorrections                                                           *
 * Authors:       Jaap Onderwaater, GSI, jacobus.onderwaater@cern.ch                              *
 *                Ilya Selyuzhenkov, GSI, ilya.selyuzhenkov@gmail.com                             *
 *                Víctor González, UCM, victor.gonzalez@cern.ch                                   *
 *                Contributors are mentioned in the code where appropriate.                       *
 * Development:   2012-2016                                                                       *
 *                                                                                                *
 * This file is part of FlowVectorCorrections, a software package that corrects Q-vector          *
 * measurements for effects of nonuniform detector acceptance. The corrections in this package    *
 * are based on publication:                                                                      *
 *                                                                                                *
 *  [1] "Effects of non-uniform acceptance in anisotropic flow measurements"                      *
 *  Ilya Selyuzhenkov and Sergei Voloshin                                                         *
 *  Phys. Rev. C 77, 034904 (2008)                                                                *
 *                                                                                                *
 * The procedure proposed in [1] is extended with the following steps:                            *
 * (*) alignment correction between subevents                                                     *
 * (*) possibility to extract the twist and rescaling corrections                                 *
 *      for the case of three detector subevents                                                  *
 *      (currently limited to the case of two “hit-only” and one “tracking” detectors)            *
 * (*) (optional) channel equalization                                                            *
 * (*) flow vector width equalization                                                             *
 *                                                                                                *
 * FlowVectorCorrections is distributed under the terms of the GNU General Public License (GPL)   *
 * (https://en.wikipedia.org/wiki/GNU_General_Public_License)                                     *
 * either version 3 of the License, or (at your option) any later version.                        *
 *                                                                                                *
 **************************************************************************************************/

/// \file QnCorrectionsQnVectors.cxx
/// \brief Implementation of Q vector classes

#include "QnCorrectionsQnVectors.h"
#include "QnCorrectionsLog.h"

/// \cond CLASSIMP
ClassImp(QnCorrectionsQnVector);
/// \endcond

/// the minimum value that will be considered as meaningful for processing
const Float_t  QnCorrectionsQnVector::fMinimumSignificantValue = 1e-6;
/// The maximum external harmonic number the framework support
const Int_t    QnCorrectionsQnVector::nMaxHarmonicNumberSupported = 15;
/// Mask for each external harmonic number
const UInt_t   QnCorrectionsQnVector::harmonicNumberMask[] =
{0x0000,0x0002,0x0004,0x0008,0x0010,0x0020,0x0040,0x0080,
 0x0100,0x0200,0x0400,0x0800,0x1000,0x2000,0x4000,0x8000};

/// Default constructor
QnCorrectionsQnVector::QnCorrectionsQnVector() : TObject() {
  memset(fQnX, 0, (nMaxHarmonicNumberSupported + 1)*sizeof(Float_t));
  memset(fQnY, 0, (nMaxHarmonicNumberSupported + 1)*sizeof(Float_t));
  fHighestHarmonic = 0;
  fHarmonicMask = 0x0000;
}

/// Normal constructor
///
/// For each harmonic number the Q vector is initialized
/// The Q vectors are organized to support external harmonic number.
/// By default the external harmonic number is always considered to
/// start by one. If no map is passed as parameter the external harmonic
/// numbers are considered as: 1, 2, ..., nNoOfHarmonic.
/// If the user wants a different assignment he has to provide an
/// ordered map, for instance: four harmonics with external harmonic numbers
/// 2, 4, 6 and 8 will require nNoOfHarmonics = 4 and harmonicMap = [2, 4, 6, 8].
///
/// A check on the asked number of harmonics is made for having it within
/// current implementation limits.
///
/// \param nNoOfHarmonics the desired number of harmonics
/// \param harmonicMap ordered array with the external number of the harmonics
QnCorrectionsQnVector::QnCorrectionsQnVector(Int_t nNoOfHarmonics, UInt_t *harmonicMap) :
    TObject() {

  memset(fQnX, 0, (nMaxHarmonicNumberSupported + 1)*sizeof(Float_t));
  memset(fQnY, 0, (nMaxHarmonicNumberSupported + 1)*sizeof(Float_t));

  /* check whether within the supported harmonic range */
  fHighestHarmonic = nNoOfHarmonics;
  if (harmonicMap != NULL) {
    fHighestHarmonic = harmonicMap[nNoOfHarmonics - 1];
  }
  if (nMaxHarmonicNumberSupported < fHighestHarmonic) {
    QnCorrectionsFatal(Form("You requested support for harmonic %d but the highest harmonic supported by the framework is currently %d",
        fHighestHarmonic, nMaxHarmonicNumberSupported));
  }
  fHarmonicMask = 0x0000;
  Int_t currentHarmonic = 0;
  for (Int_t h = 0; h < nNoOfHarmonics; h++) {
    if (harmonicMap != NULL) {
      currentHarmonic = harmonicMap[h];
    }
    else {
      currentHarmonic++;
    }
    fHarmonicMask |= harmonicNumberMask[currentHarmonic];
  }
}

/// Copy constructor
/// \param Qn the Q vector object to copy after construction
QnCorrectionsQnVector::QnCorrectionsQnVector(const QnCorrectionsQnVector &Qn) :
    TObject(Qn) {

  memcpy(fQnX, Qn.fQnX, (nMaxHarmonicNumberSupported + 1)*sizeof(Float_t));
  memcpy(fQnY, Qn.fQnY, (nMaxHarmonicNumberSupported + 1)*sizeof(Float_t));
  fHighestHarmonic = Qn.fHighestHarmonic;
  fHarmonicMask = Qn.fHarmonicMask;
}

/// Default destructor
QnCorrectionsQnVector::~QnCorrectionsQnVector() {

}

/// Activates the desired harmonic for processing
///
/// A check on the asked harmonic is made for having it within
/// current implementation limits.
///
/// If the harmonic was not active its Q vector is initialized.
///
/// \param harmonic the intended harmonic
void QnCorrectionsQnVector::ActivateHarmonic(Int_t harmonic) {
  /* check whether within the supported harmonic range */
  if (nMaxHarmonicNumberSupported < harmonic) {
    QnCorrectionsFatal(Form("You requested support for harmonic %d but the highest harmonic supported by the framework is currently %d",
        harmonic, nMaxHarmonicNumberSupported));
  }
  /* checks whether already active */
  if (fHighestHarmonic < harmonic) {
    /* not, include it */
    fHighestHarmonic = harmonic;
    fHarmonicMask |= harmonicNumberMask[harmonic];
    fQnX[harmonic] = 0.0;
    fQnY[harmonic] = 0.0;
  }
  else {
    if (fHarmonicMask & harmonicNumberMask[harmonic] == harmonicNumberMask[harmonic]) {
      /* already active */
    }
    else {
      /* activate it */
      fHarmonicMask |= harmonicNumberMask[harmonic];
      fQnX[harmonic] = 0.0;
      fQnY[harmonic] = 0.0;
    }
  }
}

/// Copy member function
///
/// The passed Q vector is copied within the current object
/// \param Qn pointer to the Q vector to be copied
void QnCorrectionsQnVector::Set(QnCorrectionsQnVector* Qn) {
  memcpy(fQnX, Qn->fQnX, (nMaxHarmonicNumberSupported + 1)*sizeof(Float_t));
  memcpy(fQnY, Qn->fQnY, (nMaxHarmonicNumberSupported + 1)*sizeof(Float_t));
  fHighestHarmonic = Qn->fHighestHarmonic;
  fHarmonicMask = Qn->fHarmonicMask;
}

/// Normalize the Q vector to unit length
///
void QnCorrectionsQnVector::Normalize() {
  for(Int_t h = 1; h < fHighestHarmonic + 1; h++){
    if (fHarmonicMask & harmonicNumberMask[h] == harmonicNumberMask[h]) {
      fQnX[h] = QxNorm(h);
      fQnY[h] = QyNorm(h);
    }
  }
}

/// Resets the Q vector values without touching the structure
void QnCorrectionsQnVector::Reset() {
  memset(fQnX, 0, (nMaxHarmonicNumberSupported + 1)*sizeof(Float_t));
  memset(fQnY, 0, (nMaxHarmonicNumberSupported + 1)*sizeof(Float_t));
}

/// Gets the event plane for the asked harmonic
///
/// A check for significant values is made. Not passing them
/// returns 0.0.
/// \param harmonic the intended harmonic number
/// \return The event plane according to \f$\frac{1}{h}\tan^{-1}{\frac{Qn_X}{Qn_Y}}\f$
Double_t QnCorrectionsQnVector::EventPlane(Int_t harmonic) const {
  if(TMath::Abs(Qx(harmonic)) < fMinimumSignificantValue && TMath::Abs(Qy(harmonic)) < fMinimumSignificantValue) {
    return 0.0;
  }
  return TMath::ATan2(Qy(harmonic), Qx(harmonic))/Double_t(harmonic);
}

/// \cond CLASSIMP
ClassImp(QnCorrectionsQnVectorBuild);
/// \endcond

/// Default constructor
QnCorrectionsQnVectorBuild::QnCorrectionsQnVectorBuild() : QnCorrectionsQnVector() {

  fSumW = 0.0;
  fN = 0;
}

/// Normal constructor
///
/// Relays on its parent for almost everything
///
/// \param nNoOfHarmonics the desired number of harmonics
/// \param harmonicMap ordered array with the external number of the harmonics
QnCorrectionsQnVectorBuild::QnCorrectionsQnVectorBuild(Int_t nNoOfHarmonics, UInt_t *harmonicMap = NULL) :
    QnCorrectionsQnVector(nNoOfHarmonics, harmonicMap) {

  fSumW = 0.0;
  fN = 0;
}

/// Copy constructor
/// \param Qn the Q vector build object to copy after construction
QnCorrectionsQnVectorBuild::QnCorrectionsQnVectorBuild(const QnCorrectionsQnVectorBuild &Qn) :
    QnCorrectionsQnVector(Qn) {

  fSumW = Qn.fSumW;
  fN = Qn.fN;
}

/// Default destructor
QnCorrectionsQnVectorBuild::~QnCorrectionsQnVectorBuild() {

}

/// Sets the X component for the considered harmonic
///
/// It should not be used. Runtime error indication.
void QnCorrectionsQnVectorBuild::SetQx(Int_t, Float_t) {

  QnCorrectionsFatal("You are using a forbidden function for a build Q vector");
}

/// Sets the Y component for the considered harmonic
///
/// It should not be used. Runtime error indication.
void QnCorrectionsQnVectorBuild::SetQy(Int_t, Float_t) {

  QnCorrectionsFatal("You are using a forbidden function for a build Q vector");
}

/// Copy member function
///
/// The passed Q vector is copied within the current object
/// \param Qn pointer to the Q vector to be copied
void QnCorrectionsQnVectorBuild::Set(QnCorrectionsQnVectorBuild* Qn) {

  QnCorrectionsQnVector::Set(Qn);
  fSumW = Qn->fSumW;
  fN = Qn->fN;
}

/// Adds a build Q vector
///
/// Warning: the possibility of a different set of harmonics for both
/// build Q vectors is currently not considered
/// \param Qn the build Q vector to add
void QnCorrectionsQnVectorBuild::Add(QnCorrectionsQnVectorBuild* Qn) {

  for(Int_t h = 1; h < fHighestHarmonic + 1; h++){
    if (fHarmonicMask & harmonicNumberMask[h] == harmonicNumberMask[h]) {
      fQnX[h] += Qn->Qx(h);
      fQnY[h] += Qn->Qy(h);
    }
  }
  fSumW += Qn->GetSumOfWeights();
  fN += Qn->GetN();
}

/// Normalizes the build Q vector for the whole harmonics set
///
/// Normalizes the build Q vector as \f$ Qn = \frac{Qn}{M} \f$.
/// A check for significant value is made. Not passing it
/// does nothing
void QnCorrectionsQnVectorBuild::NormalizeQoverM() {

  if(fSumW < fMinimumSignificantValue)
  for(Int_t h = 1; h < fHighestHarmonic + 1; h++){
    if (fHarmonicMask & harmonicNumberMask[h] == harmonicNumberMask[h]) {
      fQnX[h] += fQnX[h] / fSumW;
      fQnY[h] += fQnY[h] / fSumW;
    }
  }
}

/// Normalizes the build Q vector for the whole harmonics set
///
/// Normalizes the build Q vector as \f$ Qn = \frac{Qn}{\sqrt{M}} \f$.
/// A check for significant value is made. Not passing it
/// does nothing
void QnCorrectionsQnVectorBuild::NormalizeQoverSquareRootOfM() {

  if(fSumW < fMinimumSignificantValue)
  for(Int_t h = 1; h < fHighestHarmonic + 1; h++){
    if (fHarmonicMask & harmonicNumberMask[h] == harmonicNumberMask[h]) {
      fQnX[h] += fQnX[h] / TMath::Sqrt(fSumW);
      fQnY[h] += fQnY[h] / TMath::Sqrt(fSumW);
    }
  }
}

/// Resets the Q vector values without touching the structure
void QnCorrectionsQnVectorBuild::Reset() {

  QnCorrectionsQnVector::Reset();
  fSumW = 0.0;
  fN = 0;
}

