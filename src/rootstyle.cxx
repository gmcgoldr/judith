#include <TROOT.h>
#include <TStyle.h>
#include <TColor.h>
#include <TGaxis.h>

#include "rootstyle.h"

namespace RS {

const Int_t blue = TColor::GetColor("#332288");
const Int_t red = TColor::GetColor("#CC1100");
const Int_t green = TColor::GetColor("#117733");
const Int_t purple = TColor::GetColor("#AA4499");
const Int_t lblue = TColor::GetColor("#88CCEE");
const Int_t orange = TColor::GetColor("#DDCC77");

void setStyle() {
  TStyle* style = new TStyle("JudithStyle", "Judith style");

  // use plain black on white colors
  Int_t icol=0; // WHITE
  style->SetFrameBorderMode(icol);
  style->SetFrameFillColor(icol);
  style->SetCanvasBorderMode(icol);
  style->SetCanvasColor(icol);
  style->SetPadBorderMode(icol);
  style->SetPadColor(icol);
  style->SetStatColor(icol);

  // set the paper & margin sizes
  style->SetPaperSize(20,26);

  // set margin sizes
  style->SetPadTopMargin(0.08);
  style->SetPadRightMargin(0.08);
  style->SetPadBottomMargin(0.16);
  style->SetPadLeftMargin(0.16);

  // set title offsets (for axis label)
  style->SetTitleXOffset(1.4);
  style->SetTitleYOffset(1.4);

  // use large fonts
  Int_t font=42; // Helvetica
  Double_t tsize=0.05;
  style->SetTextFont(font);

  style->SetTextSize(tsize);
  style->SetLabelFont(font,"x");
  style->SetTitleFont(font,"x");
  style->SetLabelFont(font,"y");
  style->SetTitleFont(font,"y");
  style->SetLabelFont(font,"z");
  style->SetTitleFont(font,"z");
  
  style->SetLabelSize(tsize,"x");
  style->SetTitleSize(tsize,"x");
  style->SetLabelSize(tsize,"y");
  style->SetTitleSize(tsize,"y");
  style->SetLabelSize(tsize,"z");
  style->SetTitleSize(tsize,"z");

  // use bold lines and markers
  style->SetMarkerStyle(20);
  style->SetMarkerSize(1.2);
  style->SetHistLineWidth(2.);
  style->SetLineStyleString(2,"[12 12]"); // postscript dashes

  // get rid of error bar caps
  style->SetEndErrorSize(0.);

  // do not display any of the standard histogram decorations
  style->SetOptTitle(0);
  //style->SetOptStat(1111);
  style->SetOptStat(0);
  //style->SetOptFit(1111);
  style->SetOptFit(0);

  // put tick marks on top and RHS of plots
  style->SetPadTickX(1);
  style->SetPadTickY(1);

  style->SetHatchesSpacing(1);
  style->SetHatchesLineWidth(2);

  style->SetLegendFont(font);
  
  const Int_t ncont = 128;
  const Int_t nstops = 6;
  Double_t stops[] = { 0.000, 0.125, 0.375, 0.625, 0.875, 1.000 };
  Double_t red[]   = { 0.000, 0.000, 0.000, 1.000, 1.000, 0.500 };
  Double_t green[] = { 0.000, 0.000, 1.000, 1.000, 0.000, 0.000 };
  Double_t blue[]  = { 0.500, 1.000, 1.000, 0.000, 0.000, 0.000 };
  TColor::CreateGradientColorTable(nstops, stops, red, green, blue, ncont);

  style->SetNumberContours(ncont);

  gROOT->SetStyle("JudithStyle");
  gROOT->ForceStyle();

  TGaxis::SetMaxDigits(4);
}

}

