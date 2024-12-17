/*
Copyright 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.

 Licensed under the Apache License, Version 2.0 (the "License").
 You may not use this file except in compliance with the License.
 A copy of the License is located at

     http://www.apache.org/licenses/LICENSE-2.0

 or in the "license" file accompanying this file. This file is distributed
 on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 express or implied. See the License for the specific language governing
 permissions and limitations under the License.
*/

#ifdef __cplusplus
extern "C"
{
#endif

/* AWS Iot root certificate for region eu-central-1 */
const char aws_root_ca_pem[] = {"your_root_certificate"};

/*thing certificate:to be updated */
const char certificate_pem_crt[] = {"your_certificate"};

/* thing private key: to be updated */
const char private_pem_key[] = {"your_private_key"};

#ifdef __cplusplus
}
#endif
