#ifndef __ASN1_DIVERSION_H__
#define __ASN1_DIVERSION_H__

int encodeActivationDiversion(__u8 *dest, struct FacReqCFActivate *CFActivate);
int encodeDeactivationDiversion(__u8 *dest, struct FacReqCFDeactivate *CFDeactivate);
int encodeInterrogationDiversion(__u8 *dest, struct FacReqCFInterrogateParameters *params);
int encodeInvokeDeflection(__u8 *dest, struct FacReqCDeflection *CD);

#endif
