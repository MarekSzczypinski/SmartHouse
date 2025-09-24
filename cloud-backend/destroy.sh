#!/bin/zsh
set -eo pipefail

# Import utility functions
SCRIPT_DIR=$(dirname "$0")
source "$SCRIPT_DIR/utils.sh"

stack_name=$(get_stack_name)

log_info "Detaching certificate to thing and policy..."

thing_name=$(aws cloudformation describe-stack-resource --stack-name "$stack_name" --logical-resource-id XiaoESP32GatewayThing --query 'StackResourceDetail.PhysicalResourceId' --output text)
CERT_ARN=$(aws iot list-thing-principals --thing-name $thing_name --query 'principals[0]' --output text)
aws iot detach-thing-principal --thing-name $thing_name --principal $CERT_ARN

policy_name=$(aws cloudformation describe-stack-resource --stack-name "$stack_name" --logical-resource-id XiaoESP32GatewayPolicy --query 'StackResourceDetail.PhysicalResourceId' --output text)
aws iot detach-policy --policy-name $policy_name --target $CERT_ARN

log_info "Deleting certificate..."
CERT_ID=$(echo $CERT_ARN | cut -d'/' -f2)
aws iot update-certificate --certificate-id $CERT_ID --new-status INACTIVE
aws iot delete-certificate --certificate-id $CERT_ID

log_info "Deleting certificate.pem.crt and private.pem.key files"
rm -f certificate.pem.crt private.pem.key

log_info "Deleting stack $stack_name"
aws cloudformation delete-stack --stack-name "$stack_name"

log_info "Deletion complete!"
