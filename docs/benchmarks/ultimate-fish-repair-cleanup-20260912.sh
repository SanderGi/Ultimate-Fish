#!/usr/bin/env bash
# DESTRUCTIVE: review before executing. These commands have NOT been run.
# Removes only resources created for the 2026-09-12 tablebase repair.
# The bucket and retained disks contain the campaign's staged results/proofs.
set -euo pipefail

region=us-west-2
account=$(aws sts get-caller-identity --query Account --output text)
test "$account" = 831688117652

aws ec2 terminate-instances --region "$region" --instance-ids \
  i-033f20afbe0ec238e i-001a12c4a1506607b i-0c677b8dfb5d72b95 i-0afa42259e930b057
aws ec2 wait instance-terminated --region "$region" --instance-ids \
  i-033f20afbe0ec238e i-001a12c4a1506607b i-0c677b8dfb5d72b95 i-0afa42259e930b057

# The independent Checker/Ghost worker uses the existing Ohio quota.
aws ec2 terminate-instances --region us-east-2 --instance-ids i-06945e3dc42f324ba
aws ec2 wait instance-terminated --region us-east-2 --instance-ids i-06945e3dc42f324ba

# DeleteOnTermination=false: these encrypted gp3 volumes survive termination.
aws ec2 wait volume-available --region "$region" --volume-ids \
  vol-04ce6cb2da5179b55 vol-03aea21eaca896927 vol-06ce12bf486366bb0 vol-0e95fc6b2174923bc
aws ec2 delete-volume --region "$region" --volume-id vol-04ce6cb2da5179b55
aws ec2 delete-volume --region "$region" --volume-id vol-03aea21eaca896927
aws ec2 delete-volume --region "$region" --volume-id vol-06ce12bf486366bb0
aws ec2 delete-volume --region "$region" --volume-id vol-0e95fc6b2174923bc

aws ec2 wait volume-available --region us-east-2 --volume-ids vol-0c2a1ec5d19c92d84
aws ec2 delete-volume --region us-east-2 --volume-id vol-0c2a1ec5d19c92d84

# The private bucket is not versioned. This removes all staged files.
aws s3 rb s3://ultimate-fish-rules-repair-20260912-831688117652 --force --region "$region"

# EC2 removes the five launch-created network interfaces on termination.
aws ec2 delete-security-group --region "$region" --group-id sg-00ced6c991c16afe5
aws ec2 delete-security-group --region us-east-2 --group-id sg-06355ba62dd71b244

name=ultimate-fish-rules-repair-20260912
aws iam remove-role-from-instance-profile --instance-profile-name "$name" --role-name "$name"
aws iam delete-instance-profile --instance-profile-name "$name"
aws iam detach-role-policy --role-name "$name" \
  --policy-arn arn:aws:iam::aws:policy/AmazonSSMManagedInstanceCore
aws iam delete-role-policy --role-name "$name" --policy-name "$name"
aws iam delete-role --role-name "$name"

# The existing default VPC, subnet, AMI, and account credentials are not deleted.
