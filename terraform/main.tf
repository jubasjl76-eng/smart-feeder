# Terraform Configuration for Smart Pet Feeder API
# AWS EC2 Deployment

terraform {
  required_version = ">= 1.0.0"
  
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.0"
    }
  }
  
  backend "local" {
    path = "terraform.tfstate"
  }
}

# Variables
variable "aws_region" {
  description = "AWS region"
  type        = string
  default     = "eu-west-1"
}

variable "instance_type" {
  description = "EC2 instance type"
  type        = string
  default     = "t3.micro"
}

variable "vpc_cidr" {
  description = "VPC CIDR block"
  type        = string
  default     = "10.0.0.0/16"
}

variable "project_name" {
  description = "Project name for tagging"
  type        = string
  default     = "smart-pet-feeder"
}

# AWS Provider
provider "aws" {
  region = var.aws_region
}

# VPC
resource "aws_vpc" "main" {
  cidr_block           = var.vpc_cidr
  enable_dns_hostnames = true
  enable_dns_support   = true
  
  tags = {
    Name = "${var.project_name}-vpc"
  }
}

# Subnet
resource "aws_subnet" "public" {
  vpc_id                  = aws_vpc.main.id
  cidr_block              = "10.0.1.0/24"
  availability_zone       = "${var.aws_region}a"
  map_public_ip_on_launch = true
  
  tags = {
    Name = "${var.project_name}-subnet"
  }
}

# Internet Gateway
resource "aws_internet_gateway" "main" {
  vpc_id = aws_vpc.main.id
  
  tags = {
    Name = "${var.project_name}-igw"
  }
}

# Route Table
resource "aws_route_table" "public" {
  vpc_id = aws_vpc.main.id
  
  route {
    cidr_block = "0.0.0.0/0"
    gateway_id = aws_internet_gateway.main.id
  }
  
  tags = {
    Name = "${var.project_name}-rt"
  }
}

resource "aws_route_table_association" "public" {
  subnet_id      = aws_subnet.public.id
  route_table_id = aws_route_table.public.id
}

# Security Group
resource "aws_security_group" "api" {
  name        = "${var.project_name}-sg"
  description = "Security group for Smart Pet API"
  vpc_id      = aws_vpc.main.id
  
  # SSH access
  ingress {
    from_port   = 22
    to_port     = 22
    protocol    = "tcp"
    cidr_blocks = ["0.0.0.0/0"]
    description = "SSH"
  }
  
  # HTTP
  ingress {
    from_port   = 80
    to_port     = 80
    protocol    = "tcp"
    cidr_blocks = ["0.0.0.0/0"]
    description = "HTTP"
  }
  
  # HTTPS
  ingress {
    from_port   = 443
    to_port     = 443
    protocol    = "tcp"
    cidr_blocks = ["0.0.0.0/0"]
    description = "HTTPS"
  }
  
  # Feeder API (3002)
  ingress {
    from_port   = 3002
    to_port     = 3002
    protocol    = "tcp"
    cidr_blocks = ["0.0.0.0/0"]
    description = "Feeder API"
  }
  
  # Water API (3003)
  ingress {
    from_port   = 3003
    to_port     = 3003
    protocol    = "tcp"
    cidr_blocks = ["0.0.0.0/0"]
    description = "Water API"
  }
  
  # Outbound
  egress {
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = ["0.0.0.0/0"]
  }
  
  tags = {
    Name = "${var.project_name}-sg"
  }
}

# EC2 Instance
resource "aws_instance" "api" {
  ami           = "ami-0c55b159cbfafe1f0" # Amazon Linux 2 AMI (update for your region)
  instance_type = var.instance_type
  subnet_id     = aws_subnet.public.id
  
  vpc_security_group_ids = [aws_security_group.api.id]
  
  key_name = "your-key-pair" # Update with your key pair name
  
  user_data = <<-EOF
              #!/bin/bash
              yum update -y
              yum install -y docker
              systemctl start docker
              systemctl enable docker
              
              # Install Node.js 18
              curl -fsSL https://rpm.nodesource.com/setup_18.x | bash -
              yum install -y nodejs
              
              # Clone and setup application
              cd /opt
              git clone https://github.com/jubasjl76-eng/smart-feeder.git
              cd smart-feeder/backend
              npm install
              npm run build
              
              # Create systemd service
              cat > /etc/systemd/system/smart-feeder.service << 'SERVICE'
              [Unit]
              Description=Smart Pet Feeder API
              After=network.target
              
              [Service]
              Type=simple
              User=root
              WorkingDirectory=/opt/smart-feeder/backend
              ExecStart=/usr/bin/npm run start
              Restart=always
              
              [Install]
              WantedBy=multi-user.target
              SERVICE
              
              systemctl daemon-reload
              systemctl enable smart-feeder
              systemctl start smart-feeder
              EOF
  
  tags = {
    Name = "${var.project_name}-api-server"
  }
}

# Elastic IP
resource "aws_eip" "api" {
  instance = aws_instance.api.id
  
  tags = {
    Name = "${var.project_name}-eip"
  }
}

# Outputs
output "server_ip" {
  description = "Public IP address of the API server"
  value       = aws_eip.api.public_ip
}

output "feeder_api_url" {
  description = "Feeder API URL"
  value       = "http://${aws_eip.api.public_ip}:3002"
}

output "water_api_url" {
  description = "Water Dispenser API URL"
  value       = "http://${aws_eip.api.public_ip}:3003"
}
