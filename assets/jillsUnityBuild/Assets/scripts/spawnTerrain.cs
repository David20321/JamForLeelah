using UnityEngine;
using System.Collections;

public class spawnTerrain : MonoBehaviour {

	public float tileRange;
	float spawnRange;

	//short cuts for directions
	Vector3 north;
	Vector3 east;
	Vector3 south;
	Vector3 west;

	public GameObject thisTile;
	GameObject spawnTile;

	bool spawnNorth = false;
	bool spawnSouth = false;
	bool spawnEast = false;
	bool spawnWest = false;

	public GameObject spawnLight;
	public GameObject spawnCharacter;

	Vector3 spawnPoint;
	Vector3 spawnArea;

	float chance1;

	void Awake () {

		spawnTile = this.gameObject;
		spawnPoint = this.gameObject.transform.position;
		spawnRange = tileRange / 2;

	}

	// Use this for initialization
	void Start () {

		north = transform.TransformDirection(Vector3.forward) * tileRange;
		east = transform.TransformDirection(Vector3.right) * tileRange;
		south = transform.TransformDirection(Vector3.back) * tileRange;
		west = transform.TransformDirection(Vector3.left) * tileRange;

		spawnObjects();

	}
	
	// Update is called once per frame
	void Update () {

			if (thisTile.renderer.isVisible && spawnNorth == false) {
			//Debug.Log("Renderer is visible dawg!");
			RaycastHit hit;
			if (Physics.Raycast(transform.position, north, out hit, tileRange)) {
					//Debug.Log("I can't spawn north");
					spawnNorth = true;
				} else {
					Instantiate(spawnTile, (transform.position + north), transform.rotation);
					spawnNorth = true;
				}
			}

		if (thisTile.renderer.isVisible && spawnSouth == false) {
			//Debug.Log("Renderer is visible dawg!");
			RaycastHit hit;
			if (Physics.Raycast(transform.position, south, out hit, tileRange)) {
				//Debug.Log("I can't spawn south");
				spawnSouth = true;
			} else {
				Instantiate(spawnTile, (transform.position + south), transform.rotation);
				spawnSouth = true;
			}
		}

		if (thisTile.renderer.isVisible && spawnEast == false) {
			//Debug.Log("Renderer is visible dawg!");
			RaycastHit hit;
			if (Physics.Raycast(transform.position, east, out hit, tileRange)) {
				//Debug.Log("I can't spawn east");
				spawnEast = true;
			} else {
				Instantiate(spawnTile, (transform.position + east), transform.rotation);
				spawnEast = true;
			}
		}

		if (thisTile.renderer.isVisible && spawnWest == false) {
			//Debug.Log("Renderer is visible dawg!");
			RaycastHit hit;
			if (Physics.Raycast(transform.position, west, out hit, tileRange)) {
				//Debug.Log("I can't spawn west");
				spawnWest = true;
			} else {
				Instantiate(spawnTile, (transform.position + west), transform.rotation);
				spawnWest = true;
			}
		}
	}
	
	
	void spawnObjects () {

		float spawnACharacter = Random.Range(0, staticVariables.characterSeed);
		float spawnALight = Random.Range (0, staticVariables.lightSeed);

		var spawnIndex = Random.Range(0,9);
		float rangeX = Random.Range(-spawnRange, spawnRange);
		float rangeZ = Random.Range(-spawnRange, spawnRange);
		Vector3 newSpawnPoint = new Vector3(rangeX, 0, rangeZ);

		if (spawnALight <= 1) {

			Instantiate(spawnLight, spawnPoint, transform.rotation);
		}

		//lets spawn more than one character in a zone sometimes...
		if (spawnACharacter <= 1) {

			chance1 = Random.Range(0, 1);

			}

		Instantiate(spawnCharacter, (spawnPoint + newSpawnPoint), transform.rotation);

		if (chance1 >= 0.5f) {

			float chance2 = Random.Range(0, 1);
			float newRangeX1 = Random.Range(-spawnRange, spawnRange);
			float newRangeZ1 = Random.Range (-spawnRange, spawnRange);
			
			while (newRangeX1 <= (rangeX + 0.5f) && newRangeX1 >= (rangeX - 0.5f)) {
				//Debug.Log(rangeX + "and" + newRangeX1);
				newRangeX1 = Random.Range(-spawnRange, spawnRange);
				Debug.Log("recalculating X position");
			}
			
			while (newRangeZ1 <= (rangeZ + 0.5f) && newRangeZ1 >= (rangeZ - 0.5f)) {
				//Debug.Log(rangeX + "and" + newRangeX1);
				newRangeZ1 = Random.Range(-spawnRange, spawnRange);
				Debug.Log("recalculating Z position");
				newSpawnPoint = new Vector3 (newRangeX1, 0, newRangeZ1);
			}

			Instantiate(spawnCharacter, (spawnPoint + newSpawnPoint), transform.rotation);
		}
	}


	void OnBecameVisible() {
		enabled = true;
		//Debug.Log("I'm here yo!");
	}
}
