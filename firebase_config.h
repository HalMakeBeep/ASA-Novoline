#ifndef FIREBASE_CONFIG_H
#define FIREBASE_CONFIG_H

#include <string>

// Firebase Configuration - Prisme Project
const std::string FIREBASE_PROJECT_ID = "prisme-201b2";
const std::string FIRESTORE_API_KEY = "AIzaSyD6a18yiEjt32okATs_YBdGIRBNTfh3h9k";

// Firestore REST API endpoints
const std::string FIRESTORE_BASE_URL = "https://firestore.googleapis.com/v1/projects/" + FIREBASE_PROJECT_ID + "/databases/(default)/documents";

// Collection - Single collection for everything
const std::string COLLECTION_PRISME_ACCESS = "PrismeAppAcces";

#endif // FIREBASE_CONFIG_H
