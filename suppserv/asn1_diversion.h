#ifndef __ASN1_DIVERSION_H__
#define __ASN1_DIVERSION_H__

int encodeActivationDiversion(__u8 *dest, struct FacCFActivate *CFActivate);
int encodeDeactivationDiversion(__u8 *dest, struct FacCFDeactivate *CFDeactivate);
int encodeInterrogationDiversion(__u8 *dest, struct FacCFInterrogateParameters *params);
int encodeInvokeDeflection(__u8 *dest, struct FacCDeflection *CD);

#endif
